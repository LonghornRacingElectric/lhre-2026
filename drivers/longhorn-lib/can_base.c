#include "longhorn/can_base.h"

#include <stddef.h>
#include <stdlib.h>

#include "longhorn/can_hal.h"

static can_config_t can;

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
}

/* Overwritten in the FreeRTOS implementation of Longhorn Lib */
__attribute__((weak)) can_message_t *can_get_message_handle(uint8_t msg_size) {
  // Malloc and receive a pointer to a new object that can then be populated
  can_message_t *new_msg = malloc(sizeof(can_message_t));
  if (new_msg == NULL)
    return NULL;

  new_msg->msg = malloc(msg_size);

  if (new_msg->msg == NULL) {
    free(new_msg); // ran out of space
    return NULL;
  }

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
}

__attribute__((weak)) void
can_register_receive_packet(can_interface_t *interface,
                            can_receive_message_t *msg) {
  // add to the hash table
  uint32_t index = msg->packet_id % RECEIVE_TABLE_SIZE;
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

  return can.add_to_queue_fn(interface->handle, &tx_header, data_packet);
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
void HAL_FDCAN_RxFifo0Callback(void *hfdcan, uint32_t RxFifo0ITs);
