#include "longhorn/rtos/can.h"

#include <string.h>

#include "FreeRTOS.h"
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

// Wrapper structure to hold original unpack function and destination
typedef struct {
    void* original_dest;
    CAN_unpack_message_fn original_unpack;
} rx_wrapper_t;

// Queue item structure
typedef struct {
    uint8_t data[MAX_CAN_DATA_LEN];
    rx_wrapper_t* wrapper;
} rx_queue_item_t;

// Internal callback that runs in ISR context
static int internal_rx_callback(uint8_t* rx_data, const void* arg) {
    rx_wrapper_t* wrapper = (rx_wrapper_t*)arg;
    rx_queue_item_t item;

    // We don't know the exact length here because unpacking_fn signature doesn't
    // provide it. We assume MAX_CAN_DATA_LEN or rely on the fact that
    // can_base.c passes a 64-byte buffer.
    memcpy(item.data, rx_data, MAX_CAN_DATA_LEN);
    item.wrapper = wrapper;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(rx_queue, &item, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

    return 0;  // Return value not used by can_base.c
}

void can_rtos_init(can_config_t* config) {
    can_init(config);
    if (can_mutex == NULL) {
        can_mutex = xSemaphoreCreateMutex();
    }
    if (rx_queue == NULL) {
        rx_queue = xQueueCreate(RX_QUEUE_LENGTH, sizeof(rx_queue_item_t));
    }
    rtos_interface_count = 0;
}

void can_rtos_register_interface(can_interface_t* interface) {
    if (can_mutex != NULL) {
        xSemaphoreTake(can_mutex, portMAX_DELAY);
    }

    can_register_interface(interface);

    // Add to local list for transceiver task
    if (rtos_interface_count < MAX_INTERFACES) {
        rtos_interfaces[rtos_interface_count++] = interface;
    }

    if (can_mutex != NULL) {
        xSemaphoreGive(can_mutex);
    }
}

void can_rtos_register_send_packet(can_interface_t* interface,
                                   can_message_t* msg) {
    if (can_mutex != NULL) {
        xSemaphoreTake(can_mutex, portMAX_DELAY);
    }

    can_register_send_packet(interface, msg);

    if (can_mutex != NULL) {
        xSemaphoreGive(can_mutex);
    }
}

void can_rtos_register_receive_packet(can_interface_t* interface,
                                      can_receive_message_t* msg) {
    if (can_mutex != NULL) {
        xSemaphoreTake(can_mutex, portMAX_DELAY);
    }

    // Allocate wrapper using FreeRTOS malloc or can.malloc_fn?
    // can_base uses can.malloc_fn. Let's use pvPortMalloc for RTOS specific
    // structures.
    rx_wrapper_t* wrapper = pvPortMalloc(sizeof(rx_wrapper_t));
    if (wrapper != NULL) {
        wrapper->original_dest = msg->latest_msg;
        wrapper->original_unpack = msg->unpacking_fn;

        // Hook the callback
        msg->unpacking_fn = (CAN_unpack_message_fn)internal_rx_callback;
        msg->latest_msg = wrapper;
    }

    // Critical section to protect against ISR accessing the list while we modify
    // it
    taskENTER_CRITICAL();
    can_register_receive_packet(interface, msg);
    taskEXIT_CRITICAL();

    if (can_mutex != NULL) {
        xSemaphoreGive(can_mutex);
    }
}

cHAL_StatusTypeDef can_rtos_send_immediate(can_interface_t* interface,
                                           can_message_t* msg) {
    cHAL_StatusTypeDef status;
    if (can_mutex != NULL) {
        xSemaphoreTake(can_mutex, portMAX_DELAY);
    }

    status = can_send_immediate(interface, msg);

    if (can_mutex != NULL) {
        xSemaphoreGive(can_mutex);
    }
    return status;
}

static void transceiver_task(void* params) {
    (void)params;
    while (1) {
        if (can_mutex != NULL) {
            xSemaphoreTake(can_mutex, portMAX_DELAY);
        }

        for (int i = 0; i < rtos_interface_count; i++) {
            if (rtos_interfaces[i] != NULL) {
                can_service(rtos_interfaces[i]);
            }
        }

        if (can_mutex != NULL) {
            xSemaphoreGive(can_mutex);
        }

        vTaskDelay(pdMS_TO_TICKS(1));  // Service every 1ms
    }
}

static void receiver_task(void* params) {
    (void)params;
    rx_queue_item_t item;
    while (1) {
        if (xQueueReceive(rx_queue, &item, portMAX_DELAY) == pdTRUE) {
            if (item.wrapper != NULL && item.wrapper->original_unpack != NULL) {
                item.wrapper->original_unpack(item.data,
                                              item.wrapper->original_dest);
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
