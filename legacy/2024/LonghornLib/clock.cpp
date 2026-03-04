#include "clock.h"

static uint64_t lastTickRecorded = 0;
static uint64_t reload;
static uint64_t clockFreq;

void clock_init() {
    clockFreq = HAL_RCC_GetHCLKFreq();
    reload = clockFreq / 1000;
    HAL_SYSTICK_Config(reload);
    HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);
    HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(SysTick_IRQn);
}

float clock_getDeltaTime() {
    uint64_t tick = (reload * HAL_GetTick()) + (reload - SysTick->VAL);
    float deltaTime = ((float)(tick - lastTickRecorded)) / ((float)clockFreq);
    if(tick < lastTickRecorded) {
      return 0;
    }
    lastTickRecorded = tick;
    return deltaTime;
}

float clock_getTime() {
    uint64_t tick = (static_cast<uint64_t>(HAL_GetTick()) * reload) + (reload - SysTick->VAL);
    return ((float)tick) / ((float)clockFreq);
}