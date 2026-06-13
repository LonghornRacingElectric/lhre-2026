#include "buzzer.h"

#include "dui_can.h"
#include "main.h"

#include "cmsis_os.h"
#include <cmsis_os2.h>

#define BUZZER_HALF_PERIOD_US 500U
static osThreadAttr_t buzzer_thread_attrs = {
    .name = "Buzzer",
    .priority = osPriorityNormal,
    .stack_size = 128 * 8,
};

static void buzzer_task(void *argument);
static void delay_us(uint32_t delay);

void dui_buzzer_init(void) { osThreadNew(buzzer_task, NULL, &buzzer_thread_attrs); }

static void buzzer_task(void *argument) {
  bool speaker_high = false;

  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  while (true) {
    if (dui_r2d_buzzer_active()) {
      speaker_high = !speaker_high;
      HAL_GPIO_WritePin(SPEAKER_GPIO_Port, SPEAKER_Pin,
                        speaker_high ? GPIO_PIN_SET : GPIO_PIN_RESET);
      delay_us(BUZZER_HALF_PERIOD_US);
      osThreadYield();
    } else {
      if (speaker_high) {
        speaker_high = false;
        HAL_GPIO_WritePin(SPEAKER_GPIO_Port, SPEAKER_Pin, GPIO_PIN_RESET);
      }
      osDelay(pdMS_TO_TICKS(1));
    }
  }
}

static void delay_us(uint32_t delay) {
  uint32_t start = DWT->CYCCNT;
  uint32_t cycles = (SystemCoreClock / 1000000U) * delay;

  while ((DWT->CYCCNT - start) < cycles) {
  }
}
