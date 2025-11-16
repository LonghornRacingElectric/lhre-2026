#include "rtos/dfu.h"

#include "FreeRTOS.h"
#include "semphr.h"

SemaphoreHandle_t dfu_binary_sempahore;

void init_dfu(dfu_config config) {
    dfu_binary_sempahore = xSemaphoreCreateBinary();

    config.semaphore_release_fn = xQueueGiveFromISR;
    config.semaphore_id = dfu_binary_sempahore;
    dfu_init(config);
}

void DFU(void* argument) {
    for (;;) {
        if (xSemaphoreTake(dfu_binary_sempahore, portMAX_DELAY) == pdTRUE) {
            // we were able to get the semaphore after waiting forever
            check_dfu();
            // we never give the semaphore back because we want to block
            // until the next USB call
        }
    }
}

const osThreadAttr_t dfu_attributes = {
    .name = "DFU Helper",
    .priority = (osPriority_t)osPriorityLow,
    .stack_size = configMINIMAL_STACK_SIZE,
};

osThreadId_t dfu_start_thread() {
    return osThreadNew(DFU, NULL, &dfu_attributes);
}