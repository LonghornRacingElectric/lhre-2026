//
// Created by Dhairya Gupta on 3/3/25.
//

#include "timer.h"

#include "tim.h"


static uint64_t lastTickRecorded = 0;
static uint64_t reload;
static uint64_t clockFreq;

// deprecated
static uint32_t lib_timer_prevcycle = 0;

void lib_timer_init()
{
    lib_timer_prevcycle = HAL_GetTick();
    clockFreq = HAL_RCC_GetHCLKFreq();
    reload = clockFreq / 1000;
}

uint32_t lib_timer_delta_ms() {
    uint32_t cur = HAL_GetTick();
    uint32_t timeElapsed = cur - lib_timer_prevcycle;

    if (cur < lib_timer_prevcycle) {
        timeElapsed =
            (UINT32_MAX - lib_timer_prevcycle + cur + 1);  // thank you gemini
    }

    lib_timer_prevcycle = cur;
    return timeElapsed;
}

uint32_t lib_timer_elapsed_ms()
{
    return HAL_GetTick();
}


float lib_timer_deltaTime() {
    uint64_t tick = ((uint64_t)(HAL_GetTick()) * reload) + (reload - SysTick->VAL);
    float deltaTime = ((float)(tick - lastTickRecorded)) / ((float)clockFreq);
    if(tick < lastTickRecorded) {
        return 0;
    }
    lastTickRecorded = tick;
    return deltaTime;
}

float lib_timer_currentTime() {
    uint64_t tick = ((uint64_t)(HAL_GetTick()) * reload) + (reload - SysTick->VAL);
    return ((float)tick) / ((float)clockFreq);
}