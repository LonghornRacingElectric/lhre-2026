#include "line_lock.h"

#include "main.h"
#include "tim.h"

/**
 * Line Lock hardware PWM declarations
 */

#define LINE_LOCK_PWM_TIMER htim20
#define LINE_LOCK_PWM_CHANNEL TIM_CHANNEL_3

void line_lock_init(void) {
    uint32_t period = __HAL_TIM_GET_AUTORELOAD(&LINE_LOCK_PWM_TIMER);
    uint32_t pulse = (period + 1U) / 2U;

    __HAL_TIM_SET_COMPARE(&LINE_LOCK_PWM_TIMER, LINE_LOCK_PWM_CHANNEL, pulse);
    if (HAL_TIM_PWM_Start(&LINE_LOCK_PWM_TIMER, LINE_LOCK_PWM_CHANNEL) !=
        HAL_OK) {
        Error_Handler();
    }
}
