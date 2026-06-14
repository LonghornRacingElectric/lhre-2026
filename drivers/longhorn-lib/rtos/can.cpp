#include "longhorn/rtos/can.h"

#include <string.h>

#include "FreeRTOS.h"
#include "longhorn/rtos/logger.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#include "drivers/lal/ICan.hpp"

// Max data length for CAN FD is 64 bytes
#define RX_QUEUE_LENGTH 32

// Mutex for thread safety
static SemaphoreHandle_t can_mutex = NULL;

// Queue for received messages
static QueueHandle_t rx_queue = NULL;

// Local list of interfaces to service in the transceiver task
static can_interface_t *rtos_interfaces[MAX_INTERFACES];
static uint8_t rtos_interface_count = 0;

// Queue item structure
typedef struct {
  uint8_t data[MAX_CAN_DATA_LEN];
  can_receive_message_t *msg;
} rx_queue_item_t;

// Hook implementation that runs in ISR context
extern "C" void can_rx_hook(can_receive_message_t *msg, uint8_t *rx_data) {
  rx_queue_item_t item;

  // Copy data
  memcpy(item.data, rx_data, MAX_CAN_DATA_LEN);
  item.msg = msg;

  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xQueueSendFromISR(rx_queue, &item, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static void lal_rx_callback(const lal::CanMessage& msg, void* context) {
    can_interface_t* interface = static_cast<can_interface_t*>(context);
    // Find the message in receive_table
    uint32_t index = msg.id % RECEIVE_TABLE_SIZE;
    can_receive_message_t *rx_msg = interface->receive_table[index];
    while (rx_msg != NULL) {
        if (rx_msg->packet_id == msg.id) {
            break;
        }
        rx_msg = rx_msg->_next;
    }
    if (rx_msg == NULL) return;

    rx_queue_item_t item;
    memset(item.data, 0, MAX_CAN_DATA_LEN);
    memcpy(item.data, msg.data, msg.dlc);
    item.msg = rx_msg;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(rx_queue, &item, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Internal helper to take mutex safely even before scheduler starts
static void take_mutex(void) {
  if (can_mutex != NULL) {
    TickType_t wait_time = portMAX_DELAY;
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
      wait_time = 0;
    }
    xSemaphoreTake(can_mutex, wait_time);
  }
}

static void give_mutex(void) {
  if (can_mutex != NULL) {
    xSemaphoreGive(can_mutex);
  }
}

extern "C" void can_rtos_init(void) {
  can_config_t config = {0};
  config.malloc_fn = pvPortMalloc;
  config.free_fn = vPortFree;
  config.tick_fn = xTaskGetTickCount;
  can_init(&config);

  if (can_mutex == NULL) {
    can_mutex = xSemaphoreCreateMutex();
  }
  if (rx_queue == NULL) {
    rx_queue = xQueueCreate(RX_QUEUE_LENGTH, sizeof(rx_queue_item_t));
  }

  for (int i = 0; i < MAX_INTERFACES; i++) {
    rtos_interfaces[i] = NULL;
  }

  rtos_interface_count = 0;
}

extern "C" void can_rtos_register_interface(can_interface_t *interface) {
  take_mutex();

  can_register_interface(interface);
  auto* ican = static_cast<lal::ICan*>(interface->lal_ican);
  if (ican) {
      ican->register_rx_callback(lal_rx_callback, interface);
  }

  give_mutex();
}

extern "C" void can_rtos_start_interface(can_interface_t *interface) {
  take_mutex();

  auto* ican = static_cast<lal::ICan*>(interface->lal_ican);
  if (ican) {
      ican->start();
  }
  interface->_started = true;

  // Add to local list for transceiver task
  if (rtos_interface_count < MAX_INTERFACES) {
    rtos_interfaces[rtos_interface_count++] = interface;
  }

  give_mutex();
}

extern "C" void can_rtos_register_send_packet(can_interface_t *interface,
                                   can_message_t *msg) {
  take_mutex();
  can_register_send_packet(interface, msg);
  give_mutex();
}

extern "C" void can_rtos_register_receive_packet(can_interface_t *interface,
                                      can_receive_message_t *msg) {
  take_mutex();
  can_register_receive_packet(interface, msg);
  give_mutex();
}

extern "C" cHAL_StatusTypeDef can_rtos_send_immediate(can_interface_t *interface,
                                           can_message_t *msg) {
  take_mutex();

  auto* ican = static_cast<lal::ICan*>(interface->lal_ican);
  if (!ican) {
      give_mutex();
      return cHAL_ERROR;
  }

  lal::CanMessage lal_msg;
  lal_msg.id = msg->packet_id;
  lal_msg.is_extended = false;
  lal_msg.dlc = msg->dlc;
  
  uint8_t data_packet[MAX_CAN_DATA_LEN] = {0};
  msg->packing_fn(msg->msg, data_packet);
  memcpy(lal_msg.data, data_packet, lal_msg.dlc);

  interface->_messages_sent++;
  bool success = ican->send(lal_msg);
  if (!success) {
      interface->_error_occurred = true;
      interface->_error_code_send = cHAL_ERROR;
  }

  give_mutex();
  return success ? cHAL_OK : cHAL_ERROR;
}

static void transceiver_task(void *params) {
  (void)params;
  while (1) {
    take_mutex();

    for (int i = 0; i < rtos_interface_count; i++) {
      if (rtos_interfaces[i] != NULL) {
        can_interface_t *interface = rtos_interfaces[i];
        can_message_t *cur = interface->_head;

        while (cur) {
          if (cur->_is_scheduled && cur->period_ms > 0 &&
              (xTaskGetTickCount() - cur->_last_tx_time_ms) >= cur->period_ms) {

            auto* ican = static_cast<lal::ICan*>(interface->lal_ican);
            if (ican) {
                lal::CanMessage lal_msg;
                lal_msg.id = cur->packet_id;
                lal_msg.is_extended = false;
                lal_msg.dlc = cur->dlc;
                uint8_t data_packet[MAX_CAN_DATA_LEN] = {0};
                cur->packing_fn(cur->msg, data_packet);
                memcpy(lal_msg.data, data_packet, lal_msg.dlc);

                if (ican->send(lal_msg)) {
                    cur->_last_tx_time_ms = xTaskGetTickCount();
                    interface->_messages_sent++;
                } else {
                    interface->dropped_packets++;
                    cur->_last_tx_time_ms = xTaskGetTickCount();
                }
            }
          }
          cur = cur->_next;
        }
      }
    }

    give_mutex();

    vTaskDelay(pdMS_TO_TICKS(CAN_FREQ_GCD)); // Service at greatest denominator of packets
  }
}

static void receiver_task(void *params) {
  (void)params;
  rx_queue_item_t item;
  while (1) {
    if (xQueueReceive(rx_queue, &item, portMAX_DELAY) == pdTRUE) {
      if (item.msg != NULL && item.msg->unpacking_fn != NULL) {
        item.msg->unpacking_fn(item.data, item.msg->latest_msg);
      }
    }
  }
}

extern "C" BaseType_t can_rtos_start_transceiver_task(UBaseType_t priority) {
  return xTaskCreate(transceiver_task, "CAN_Tx", 512, NULL, priority, NULL);
}

extern "C" BaseType_t can_rtos_start_receiver_task(UBaseType_t priority) {
  return xTaskCreate(receiver_task, "CAN_Rx", 512, NULL, priority, NULL);
}
