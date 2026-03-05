#include "led.h"
#include "tim.h"

void led_init() {
#ifdef STM32L431xx
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);
#else
#ifndef STM32H7A3xxQ
  HAL_TIM_PWM_Start(&htim5, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim5, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim5, TIM_CHANNEL_3);
#endif
#endif
}

static void led_setInt(uint8_t r, uint8_t g, uint8_t b) {
#ifdef STM32L431xx
  TIM2->CCR1 = r;
  TIM2->CCR2 = g;
  TIM2->CCR3 = b;
#else
#ifndef STM32H7A3xxQ
  TIM5->CCR1 = b;
  TIM5->CCR2 = r;
  TIM5->CCR3 = g;
#endif
#endif
}

void led_set(float r, float g, float b) {
  led_setInt((uint8_t) (r * 255), (uint8_t) (g * 255), (uint8_t) (b * 255));
}

void led_off() {
  led_setInt(0, 0, 0);
}

void led_rainbow(float deltaTime) {
  static uint8_t r = 255, g = 0, b = 0;
  static float timer = 0;

  uint32_t count;
  timer += deltaTime * 3.0f * 255.0f;
  if (timer > 3.0f) {
    // one cycle every 1.5 seconds
    count = static_cast<uint32_t>(timer / 3.0f);
    timer = 0;
    if(count > 10)
      count = 1;
  } else {
    return;
  }

  for(uint32_t i = 0; i < count; i++) {
    if ((r && g) || (!g && !b)) {
      r--;
      g++;
    } else if ((g && b) || (!b && !r)) {
      g--;
      b++;
    } else {
      b--;
      r++;
    }
  }
  led_setInt(r / 4, g / 2, b / 2);
}