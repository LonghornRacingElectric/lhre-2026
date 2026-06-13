#include "buzzer.h"

#include "dui_can.h"
#include "main.h"
#include "tim.h"

// Cap continuous buzzing at 4000 half-periods (2000ms) so a stuck/runaway
// "buzzer active" signal can't drive the speaker forever.
#define BUZZER_MAX_ACTIVE_PERIODS 4000U

static bool speaker_high = false;
static uint32_t active_periods = 0;
static volatile uint32_t tick_count = 0;
static volatile uint32_t active_tick_count = 0;

void dui_buzzer_init(void) { HAL_TIM_Base_Start_IT(&htim3); }

uint32_t dui_buzzer_tick_count(void) { return tick_count; }
uint32_t dui_buzzer_active_tick_count(void) { return active_tick_count; }

// Called from HAL_TIM_PeriodElapsedCallback on every TIM3 update (500us).
void dui_buzzer_tick(void) {
  tick_count++;

  bool buzzer_active = dui_r2d_buzzer_active();

  if (buzzer_active && active_periods < BUZZER_MAX_ACTIVE_PERIODS) {
    active_tick_count++;
    speaker_high = !speaker_high;
    HAL_GPIO_WritePin(SPEAKER_GPIO_Port, SPEAKER_Pin,
                      speaker_high ? GPIO_PIN_SET : GPIO_PIN_RESET);
    active_periods++;
  } else {
    if (!buzzer_active) {
      active_periods = 0;
    }
    speaker_high = false;
    HAL_GPIO_WritePin(SPEAKER_GPIO_Port, SPEAKER_Pin, GPIO_PIN_RESET);
  }
}
