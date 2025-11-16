#include "rtos/dfu.h"

#include "FreeRTOS.h"
#include "semphr.h"

SemaphoreHandle_t dfu_binary_sempahore;

/**
 * @brief Function to bridge non-RTOS to allow yielding from ISR
 *
 */
void yield() { portYIELD_FROM_ISR(true); }

void init_dfu(dfu_config config) {
    dfu_binary_sempahore = xSemaphoreCreateBinary();

    config.semaphore_release_fn = (SemaphoreRelease_Fn)xQueueGiveFromISR;
    config.semaphore_id = dfu_binary_sempahore;
    config.yield_fn = yield;
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
    .priority = (osPriority_t)
        osPriorityRealtime,  // MAX priority so that IF we ever send an update
                             // command, we WILL go into reset mode IMMEDIATELY,
                             // not wait for other tasks
    .stack_size = configMINIMAL_STACK_SIZE,
};

osThreadId_t dfu_start_thread() {
    return osThreadNew(DFU, NULL, &dfu_attributes);
}