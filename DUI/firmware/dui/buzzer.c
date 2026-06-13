#include "buzzer.h"

#include "dui_can.h"
#include "main.h"

#include "cmsis_os.h"
#include <cmsis_os2.h>

#define BUZZER_HALF_PERIOD_MS 1U
#define BUZZER_MAX_ACTIVE_MS 1500U
static osThreadAttr_t buzzer_thread_attrs = {
    .name = "Buzzer",
    .priority = osPriorityNormal,
    .stack_size = 128 * 8,
};

static void buzzer_task(void *argument);

void dui_buzzer_init(void) { osThreadNew(buzzer_task, NULL, &buzzer_thread_attrs); }

static void buzzer_task(void *argument) {
  bool speaker_high = false;
  bool was_active = false;
  uint32_t active_start_ms = 0;

  while (true) {
    bool active = dui_r2d_buzzer_active();
    uint32_t now_ms = HAL_GetTick();

    if (active && !was_active) {
      active_start_ms = now_ms;
    }

    if (active && (now_ms - active_start_ms < BUZZER_MAX_ACTIVE_MS)) {
      speaker_high = !speaker_high;
      HAL_GPIO_WritePin(SPEAKER_GPIO_Port, SPEAKER_Pin,
                        speaker_high ? GPIO_PIN_SET : GPIO_PIN_RESET);
      osDelay(pdMS_TO_TICKS(BUZZER_HALF_PERIOD_MS));
    } else {
      if (speaker_high) {
        speaker_high = false;
        HAL_GPIO_WritePin(SPEAKER_GPIO_Port, SPEAKER_Pin, GPIO_PIN_RESET);
      }
      osDelay(pdMS_TO_TICKS(1));
    }

    was_active = active;
  }
}
