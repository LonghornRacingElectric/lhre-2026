#include "longhorn/rtos/can.h"

#include <string.h>

#include "FreeRTOS.h"
#include "longhorn/rtos/logger.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

// Max data length for CAN FD is 64 bytes
#define MAX_CAN_DATA_LEN 64
#define RX_QUEUE_LENGTH 32
#define MAX_INTERFACES 2  // Matching can_base.c

// Mutex for thread safety
static SemaphoreHandle_t can_mutex = NULL;

// Queue for received messages
static QueueHandle_t rx_queue = NULL;

// Local list of interfaces to service in the transceiver task
static can_interface_t* rtos_interfaces[MAX_INTERFACES];
static uint8_t rtos_interface_count = 0;

// Queue item structure
typedef struct {
    uint8_t data[MAX_CAN_DATA_LEN];
    can_receive_message_t* msg;
} rx_queue_item_t;

// Hook implementation that runs in ISR context
void can_rx_hook(can_receive_message_t* msg, uint8_t* rx_data) {
    rx_queue_item_t item;

    // Copy data
    memcpy(item.data, rx_data, MAX_CAN_DATA_LEN);
    item.msg = msg;

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

void can_rtos_init(can_config_t* config) {
    can_init(config);
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

void can_rtos_register_interface(can_interface_t* interface) {
    take_mutex();

    can_register_interface(interface);

    // Add to local list for transceiver task
    if (rtos_interface_count < MAX_INTERFACES) {
        rtos_interfaces[rtos_interface_count++] = interface;
    }

    give_mutex();
}

void can_rtos_register_send_packet(can_interface_t* interface,
                                   can_message_t* msg) {
    take_mutex();

    can_register_send_packet(interface, msg);

    give_mutex();
}

void can_rtos_register_receive_packet(can_interface_t* interface,
                                      can_receive_message_t* msg) {
    take_mutex();

    // Critical section to protect against ISR accessing the list while we
    // modify it
    taskENTER_CRITICAL();
    can_register_receive_packet(interface, msg);
    taskEXIT_CRITICAL();

    give_mutex();
}

cHAL_StatusTypeDef can_rtos_send_immediate(can_interface_t* interface,
                                           can_message_t* msg) {
    cHAL_StatusTypeDef status;
    take_mutex();

    status = can_send_immediate(interface, msg);

    give_mutex();
    return status;
}

static void transceiver_task(void* params) {
    (void)params;
    while (1) {
        take_mutex();

        for (int i = 0; i < rtos_interface_count; i++) {
            if (rtos_interfaces[i] != NULL) {
                can_service(rtos_interfaces[i]);
            }
        }

        give_mutex();

        vTaskDelay(pdMS_TO_TICKS(CAN_FREQ_GCD));  // Service every 1ms
    }
}

static void receiver_task(void* params) {
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

BaseType_t can_rtos_start_transceiver_task(UBaseType_t priority) {
    return xTaskCreate(transceiver_task, "CAN_Tx", 256, NULL, priority, NULL);
}

BaseType_t can_rtos_start_receiver_task(UBaseType_t priority) {
    return xTaskCreate(receiver_task, "CAN_Rx", 256, NULL, priority, NULL);
}
