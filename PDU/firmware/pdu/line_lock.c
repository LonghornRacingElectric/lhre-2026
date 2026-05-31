#include "line_lock.h"

#include "main.h"
#include "tim.h"

/**
 * Line Lock hardware PWM declarations
 */

#define LINE_LOCK_PWM_TIMER htim20
#define LINE_LOCK_PWM_CHANNEL TIM_CHANNEL_3
#define LINE_LOCK_PWM_DUTY_PERCENT 50U

#if LINE_LOCK_PWM_DUTY_PERCENT > 100U
#error "LINE_LOCK_PWM_DUTY_PERCENT must be 0-100"
#endif

static void line_lock_set_duty_percent(uint32_t duty_percent) {
    uint32_t period_counts = __HAL_TIM_GET_AUTORELOAD(&LINE_LOCK_PWM_TIMER) + 1U;
    uint32_t pulse = (period_counts * duty_percent) / 100U;

    __HAL_TIM_SET_COMPARE(&LINE_LOCK_PWM_TIMER, LINE_LOCK_PWM_CHANNEL, pulse);
}

void line_lock_init(void) {
    line_lock_set_duty_percent(LINE_LOCK_PWM_DUTY_PERCENT);
    if (HAL_TIM_PWM_Start(&LINE_LOCK_PWM_TIMER, LINE_LOCK_PWM_CHANNEL) !=
        HAL_OK) {
        Error_Handler();
    }
}
