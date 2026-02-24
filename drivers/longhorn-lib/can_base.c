#include "longhorn/can_base.h"

#include <stddef.h>
#include <stdlib.h>

#include "longhorn/can_hal.h"
#include "longhorn/led_base.h"

static can_config_t can;

static can_interface_t *interfaces[MAX_INTERFACES];

#define TX_FIFO_TIMEOUT_MS 1

static uint8_t interface_count = 0;

void can_init(can_config_t *config) {
  // set our can library to use the correct functions
  can = *config;
}

void can_register_interface(can_interface_t *interface) {
  // Initialize the CAN interface (but don't start it yet)
  can.init_fn(interface->handle);

  if (interface_count >= MAX_INTERFACES) {
    // too many interfaces registered
    return;
  }

  interfaces[interface_count] = interface;
  interface_count++;
}

void can_start_interface(can_interface_t *interface) {
  // Enable RX notifications
  can.noti_fn(interface->handle, NEW_MESSAGE_FIFO0, 0);

  // Start the CAN peripheral (must be called AFTER all filters/packets are
  // registered)
  can.start_fn(interface->handle);

  // note that the interface started
  interface->_started = true;
}

/* Overwritten in the FreeRTOS implementation of Longhorn Lib */
__attribute__((weak)) can_message_t *
can_get_message_handle(void *msg, uint32_t packet_id, uint16_t freq,
                       uint8_t dlc, CAN_pack_message_fn packing_fn) {
  if (dlc <= 0) {
    led_disable();
    return NULL;
  }
  // Malloc and receive a pointer to a new object that can then be populated
  can_message_t *new_msg = can.malloc_fn(sizeof(can_message_t));
  if (new_msg == NULL)
    return NULL;

  new_msg->msg = msg;
  new_msg->dlc = dlc;
  new_msg->packet_id = packet_id;
  new_msg->period_ms = freq;
  new_msg->packing_fn = packing_fn;

  new_msg->id_type = FDCAN_STANDARD_ID;
  new_msg->_next = NULL;
  new_msg->_is_scheduled = false;

  return new_msg;
}

__attribute__((weak)) void can_register_send_packet(can_interface_t *interface,
                                                    can_message_t *msg) {
  msg->_is_scheduled = true;

  if (msg->period_ms > 0) {
    uint32_t msg_count = 0;
    can_message_t *cur = interface->_head;
    while (cur != NULL) {
      msg_count++;
      cur = cur->_next;
    }

    // Calculate staggered start time
    // We modulo by period_ms so we don't push the start time too far into
    // the future if we have many messages
    uint32_t phase_offset = (msg_count * PHASE_STAGGER_MS) % msg->period_ms;

    // Last = Now + offset - Period
    // Means that we're going to be sending at some phased offset now
    msg->_last_tx_time_ms = can.tick_fn() + phase_offset - msg->period_ms;
  } else {
    msg->_last_tx_time_ms = can.tick_fn();
  }

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

__attribute__((weak)) can_receive_message_t *
can_get_receive_message_handle(void *msg, uint32_t packet_id,
                               CAN_unpack_message_fn unpacking_fn) {
  can_receive_message_t *new_msg = can.malloc_fn(sizeof(can_receive_message_t));
  if (new_msg == NULL)
    return NULL;

  new_msg->latest_msg = msg;
  new_msg->unpacking_fn = unpacking_fn;
  new_msg->packet_id = packet_id;
  new_msg->_latest_rx_ms = can.tick_fn();

  new_msg->_next = NULL;

  return new_msg;
}

__attribute__((weak)) void
can_register_receive_packet(can_interface_t *interface,
                            can_receive_message_t *msg) {
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

  // Configure the hardware filter. This should be called BEFORE
  // can_start_interface(). The peripheral must be in init mode (not started)
  // for filter configuration to be safe.
  cFDCAN_FilterTypeDef sFilterConfig;
  sFilterConfig.IdType = FDCAN_STANDARD_ID;
  sFilterConfig.FilterIndex = interface->_filter_index;
  sFilterConfig.FilterType = FDCAN_FILTER_DUAL;
  sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  sFilterConfig.FilterID1 = msg->packet_id;
  sFilterConfig.FilterID2 = msg->packet_id;

  can.add_filter_fn(interface->handle, &sFilterConfig);
  interface->_filter_index++;
}

static uint8_t data_packet[MAX_CAN_DATA_LEN];

cHAL_StatusTypeDef can_send_immediate(can_interface_t *interface,
                                      can_message_t *msg) {

  uint32_t start = can.tick_fn();
  while (can.get_tx_fifo_free_level_fn(interface->handle) == 0) {
    if (can.tick_fn() - start >= TX_FIFO_TIMEOUT_MS) {
      interface->_error_occurred = true;
      interface->dropped_packets++;
      return cHAL_TIMEOUT;
    }
  }

  cFDCAN_TxHeaderTypeDef tx_header;
  tx_header.DataLength = msg->dlc;
  tx_header.IdType = msg->id_type;
  tx_header.Identifier = msg->packet_id;
  tx_header.TxFrameType = FDCAN_DATA_FRAME;
  tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  tx_header.BitRateSwitch = FDCAN_BRS_OFF;
  tx_header.FDFormat = FDCAN_CLASSIC_CAN;
  tx_header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;

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
    if (cur->_is_scheduled && cur->period_ms > 0 &&
        (can.tick_fn() - cur->_last_tx_time_ms) >= cur->period_ms) {

      cHAL_StatusTypeDef error = can_send_immediate(interface, cur);

      if (error == cHAL_OK) {
        cur->_last_tx_time_ms = can.tick_fn();
      } else {
        interface->dropped_packets++;
        // still update so that we don't spam retry this packet in future runs,
        // just treat as dropped
        cur->_last_tx_time_ms = can.tick_fn();
      }
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
      uint8_t rx_data[MAX_CAN_DATA_LEN] = {0};
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
          interface->receive_table[rx_header.Identifier % RECEIVE_TABLE_SIZE];

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
      can_rx_hook(msg, rx_data);

      // update the latest rx time
      msg->_latest_rx_ms = can.tick_fn();
    }
  }
}

// https://community.st.com/t5/stm32-mcus/how-to-recover-from-bus-off-state-with-fdcan-on-stm32-mcus/ta-p/851596
void HAL_FDCAN_ErrorStatusCallback(void *hfdcan, uint32_t ErrorStatusITs) {
  if ((ErrorStatusITs & cFDCAN_IT_BUS_OFF) != 0) { // If Bus-Off error occurred
    *can.cccr_reg &= ~can.init_bit; // Clear INIT bit to recover from Bus-Off
  }
}

__attribute__((weak)) void can_rx_hook(can_receive_message_t *msg,
                                       uint8_t *rx_data) {
  msg->unpacking_fn(rx_data, msg->latest_msg);
}

void can_reset_internals(void) {
  interface_count = 0;
  for (int i = 0; i < MAX_INTERFACES; i++) {
    interfaces[i] = NULL;
  }
}

bool message_timed_out(can_receive_message_t *msg, uint32_t timeout_ms) {
  if ((can.tick_fn() - msg->_latest_rx_ms) >= timeout_ms) {
    msg->timed_out = true;
  } else {
    msg->timed_out = false;
  }

  return msg->timed_out;
}

bool message_timed_out_sticky(can_receive_message_t *msg, uint32_t timeout_ms) {
  if (msg->timed_out) {
    return true;
  }

  if ((can.tick_fn() - msg->_latest_rx_ms) >= timeout_ms) {
    msg->timed_out = true;
  }
  return msg->timed_out;
}
