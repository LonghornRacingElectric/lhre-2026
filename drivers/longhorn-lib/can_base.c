#include "longhorn/can_base.h"

#include <stddef.h>
#include <stdlib.h>

#include "longhorn/can_hal.h"

#define MAX_INTERFACES 2

static can_config_t can;

static can_interface_t *interfaces[MAX_INTERFACES];

static uint8_t interface_count = 0;

void can_init(can_config_t *config) {
    // set our can library to use the correct functions
    can = *config;
}

void can_register_interface(can_interface_t *interface) {
    // Initialize the CAN interface
    can.init_fn(interface->handle);

    // Do a callback for when a message is received
    can.noti_fn(interface->handle, NEW_MESSAGE_FIFO0, 0);

    // start the CAN peripheral
    can.start_fn(interface->handle);

    // note that the interface started
    interface->_started = true;

    // keep track of the interface
    interfaces[interface_count] = interface;
    interface_count++;
}

/* Overwritten in the FreeRTOS implementation of Longhorn Lib */
__attribute__((weak)) can_message_t *can_get_message_handle(void *msg) {
    // Malloc and receive a pointer to a new object that can then be populated
    can_message_t *new_msg = can.malloc_fn(sizeof(can_message_t));
    if (new_msg == NULL) return NULL;

    new_msg->msg = msg;

    new_msg->_next = NULL;
    new_msg->_is_scheduled = false;

    return new_msg;
}

__attribute__((weak)) void can_register_send_packet(can_interface_t *interface,
                                                    can_message_t *msg) {
    // Last time it was sent is right now
    msg->_is_scheduled = true;
    msg->_last_tx_time_ms = can.tick_fn();

    if (!interface->_head) {
        // we haven't yet created anything
        interface->_head = msg;
        interface->_tail = msg;
    } else {
        // we already have messages registered, just add this
        interface->_tail->_next = msg;
        interface->_tail = msg;
    }

    interface->_filter_index++;
}

__attribute__((weak)) can_receive_message_t *can_get_receive_message_handle(
    void *msg) {
    can_receive_message_t *new_msg =
        can.malloc_fn(sizeof(can_receive_message_t));
    if (new_msg == NULL) return NULL;

    new_msg->latest_msg = msg;

    new_msg->_next = NULL;

    return new_msg;
}

__attribute__((weak)) void can_register_receive_packet(
    can_interface_t *interface, can_receive_message_t *msg) {
    // add to the hash table
    uint32_t index = msg->packet_id % RECEIVE_TABLE_SIZE;

    if (interface->receive_table[index] != NULL) {
        // we already have a message registered for this ID
        // add to the linked list at this spot (bucket method)
        can_receive_message_t *cur = interface->receive_table[index];

        while (cur->_next != NULL) {
            cur = cur->_next;
        }

        cur->_next = msg;
    } else {
        interface->receive_table[index] = msg;
    }

    msg->_latest_rx_ms = can.tick_fn();
    msg->timed_out = false;

    can.stop_fn(interface->handle);
    cFDCAN_FilterTypeDef sFilterConfig;
    sFilterConfig.IdType = FDCAN_STANDARD_ID;
    sFilterConfig.FilterIndex = 0;
    sFilterConfig.FilterType = FDCAN_FILTER_DUAL;
    sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    sFilterConfig.FilterID1 = msg->packet_id;
    sFilterConfig.FilterID2 = msg->packet_id;

    can.add_filter_fn(interface->handle, &sFilterConfig);
    can.start_fn(interface->handle);
}

cHAL_StatusTypeDef can_send_immediate(can_interface_t *interface,
                                      can_message_t *msg) {
    cFDCAN_TxHeaderTypeDef tx_header;
    tx_header.DataLength = msg->dlc;
    tx_header.IdType = msg->id_type;
    tx_header.Identifier = msg->packet_id;
    tx_header.TxFrameType = FDCAN_DATA_FRAME;
    tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    tx_header.BitRateSwitch = FDCAN_BRS_OFF;
    tx_header.FDFormat = FDCAN_CLASSIC_CAN;
    tx_header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;

    // max CAN FD size
    uint8_t data_packet[64] = {0};

    msg->packing_fn(msg->msg, data_packet);
    interface->_messages_sent++;

    cHAL_StatusTypeDef error =
        can.add_to_queue_fn(interface->handle, &tx_header, data_packet);
    if (error != cHAL_OK) {
        interface->_error_occurred = true;
        interface->_error_code_send = error;
    }

    return error;
}

void can_service(can_interface_t *interface) {
    can_message_t *cur = interface->_head;

    while (cur) {
        // we have a potential message we need to send
        if (cur->_is_scheduled &&
            (can.tick_fn() - cur->_last_tx_time_ms) >= cur->period_ms) {
            // send the message
            cHAL_StatusTypeDef error = can_send_immediate(interface, cur);

            if (error == cHAL_ERROR) {
                // keep track of how many packets were dropped
                interface->dropped_packets++;
            }

            // update so that we know when it was last sent
            cur->_last_tx_time_ms = can.tick_fn();
        }

        cur = cur->_next;
    }
}

/**
 * @brief Overwrite the HAL's weak definition for callback
 *
 * @param hfdcan
 * @param RxFifo0ITs
 */
void HAL_FDCAN_RxFifo0Callback(void *hfdcan, uint32_t RxFifo0ITs) {
    if ((RxFifo0ITs & NEW_MESSAGE_FIFO0) == 0) {
        // callback didn't fire for a new message
        return;
    }
    // whenever we see a new packet come in, we need to see whawt handle it was
    // make sure it exists in our table, and then call the unpack function
    for (int i = 0; i < interface_count; i++) {
        if (interfaces[i]->handle == hfdcan) {
            // we found the interface
            can_interface_t *interface = interfaces[i];

            // see what message it was
            cFDCAN_RxHeaderTypeDef rx_header;
            uint8_t rx_data[64] = {0};
            cHAL_StatusTypeDef status = can.get_rx_message_fn(
                interface->handle, FDCAN_RX_FIFO0, &rx_header, rx_data);

            if (status != cHAL_OK) {
                interface->_error_occurred = true;
                interface->_error_code_receive = status;
                continue;
            }

            interfaces[i]->_last_id_received = rx_header.Identifier;

            // make sure it exists in our table
            can_receive_message_t *msg =
                interface->receive_table[rx_header.Identifier %
                                         RECEIVE_TABLE_SIZE];

            if (msg == NULL) {
                // we don't have a message registered for this ID
                continue;
            } else {
                // we have a message registered for this ID OR a clash on our
                // hash table
                // we need to check the linked list
                while (msg != NULL) {
                    if (msg->packet_id == rx_header.Identifier) {
                        // we found the message
                        break;
                    }
                    msg = msg->_next;
                }
                if (msg == NULL) {
                    // we don't have a message registered for this ID
                    continue;
                }
            }

            // call the unpack function
            msg->unpacking_fn(rx_data, msg->latest_msg);

            // update the latest rx time
            msg->_latest_rx_ms = can.tick_fn();
        }
    }
}

void can_reset_internals(void) {
    interface_count = 0;
    for (int i = 0; i < MAX_INTERFACES; i++) {
        interfaces[i] = NULL;
    }
}
