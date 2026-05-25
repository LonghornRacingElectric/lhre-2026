#include "line_lock.h"

#include "cmsis_os.h"
#include "main.h"

/**
 * Line Lock Software PWM Declarations
 */

#define LINE_LOCK_PWM_PERIOD_US 100U
#define LINE_LOCK_PWM_DUTY_CYCLE 0.5f
#define LINE_LOCK_ON_TIME_US \
    ((uint32_t)((float)LINE_LOCK_PWM_PERIOD_US * LINE_LOCK_PWM_DUTY_CYCLE))

static const uint32_t line_lock_on_time_us = LINE_LOCK_ON_TIME_US;
static const uint32_t line_lock_off_time_us =
    LINE_LOCK_PWM_PERIOD_US - LINE_LOCK_ON_TIME_US;

static osThreadAttr_t lineLockTask_attributes = {
    .name = "lineLockTask",
    .priority = (osPriority_t)osPriorityLow,
    .stack_size = 128 * 4,
};

static void delay_us(uint32_t delay) {
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = delay * (HAL_RCC_GetHCLKFreq() / 1000000U);

    while ((DWT->CYCCNT - start) < ticks) {
    }
}

static void line_lock_delay_init(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void line_lock_task(void *argument) {
    (void)argument;

    while (1) {
        HAL_GPIO_WritePin(Line_Lock_EN_GPIO_Port, Line_Lock_EN_Pin,
                          GPIO_PIN_SET);
        delay_us(line_lock_on_time_us);
        HAL_GPIO_WritePin(Line_Lock_EN_GPIO_Port, Line_Lock_EN_Pin,
                          GPIO_PIN_RESET);
        delay_us(line_lock_off_time_us);
    }
}

void line_lock_init(void) {
    line_lock_delay_init();

    osThreadNew(line_lock_task, NULL, &lineLockTask_attributes);
}
