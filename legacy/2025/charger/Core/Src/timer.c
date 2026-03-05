//
// Created by Dhairya Gupta on 3/3/25.
//

#include "timer.h"
#include "tim.h"

static uint32_t lib_timer_prevcycle = 0;

void lib_timer_init() { lib_timer_prevcycle = HAL_GetTick(); }

uint32_t lib_timer_ms_elapsed() {
  uint32_t cur = HAL_GetTick();
  uint32_t timeElapsed = cur - lib_timer_prevcycle;

  if (cur < lib_timer_prevcycle) {
    timeElapsed =
        (UINT32_MAX - lib_timer_prevcycle + cur + 1); // thank you gemini
  }

  lib_timer_prevcycle = cur;
  return timeElapsed;
}