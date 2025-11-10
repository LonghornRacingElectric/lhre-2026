#include "rtos/led.h"

#include <math.h>

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "led_base.h"

void RainbowLED(void* argument) {
    const uint32_t periodTicks = pdMS_TO_TICKS(33);
    const float secondsPerTick = 1.0f / (float)configTICK_RATE_HZ;
    uint32_t lastRunTime = osKernelGetTickCount();
    uint32_t nextWakeTime = lastRunTime;

    for (;;) {
        // absolute time that the rainbow task should be woken up again
        nextWakeTime += periodTicks;
        osDelayUntil(nextWakeTime);

        uint32_t currentRunTime = osKernelGetTickCount();
        uint32_t elapsedTicks = currentRunTime - lastRunTime;
        float elapsedSeconds = (float)elapsedTicks * secondsPerTick;

        // rainbow with however much time actually elapsed
        led_rainbow(elapsedSeconds);
        lastRunTime = currentRunTime;
    }
}

const osThreadAttr_t led_attributes = {
    .name = "LED Rainbow",
    .priority = (osPriority_t)osPriorityNormal,
    .stack_size = configMINIMAL_STACK_SIZE * 2};

osThreadId_t led_start_thread() {
    return osThreadNew(RainbowLED, NULL, &led_attributes);
}
