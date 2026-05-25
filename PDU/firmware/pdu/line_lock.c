#include "line_lock.h"

#include "cmsis_os.h"
#include "main.h"

/**
 * Line Lock Software PWM Declarations
 */

#define LINE_LOCK_GPIO_PORT GPIOF
#define LINE_LOCK_GPIO_PIN GPIO_PIN_2
#define LINE_LOCK_PWM_PERIOD_US 1000U
#define LINE_LOCK_TASK_DELAY_MS 100U

static volatile uint32_t line_lock_on_time_us = LINE_LOCK_PWM_PERIOD_US;

static osThreadAttr_t lineLockTask_attributes = {
    .name = "lineLockTask",
    .priority = (osPriority_t)osPriorityNormal,
    .stack_size = 128 * 4,
};

static float clamp_pwm(float percentage) {
    if (percentage < 0.0f) {
        return 0.0f;
    }

    if (percentage > 1.0f) {
        return 1.0f;
    }

    return percentage;
}

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
        uint32_t on_time_us = line_lock_on_time_us;

        if (on_time_us >= LINE_LOCK_PWM_PERIOD_US) {
            HAL_GPIO_WritePin(LINE_LOCK_GPIO_PORT, LINE_LOCK_GPIO_PIN,
                              GPIO_PIN_SET);
            osDelay(LINE_LOCK_TASK_DELAY_MS);
        } else if (on_time_us == 0U) {
            HAL_GPIO_WritePin(LINE_LOCK_GPIO_PORT, LINE_LOCK_GPIO_PIN,
                              GPIO_PIN_RESET);
            osDelay(LINE_LOCK_TASK_DELAY_MS);
        } else {
            HAL_GPIO_WritePin(LINE_LOCK_GPIO_PORT, LINE_LOCK_GPIO_PIN,
                              GPIO_PIN_SET);
            delay_us(on_time_us);
            HAL_GPIO_WritePin(LINE_LOCK_GPIO_PORT, LINE_LOCK_GPIO_PIN,
                              GPIO_PIN_RESET);
            delay_us(LINE_LOCK_PWM_PERIOD_US - on_time_us);
        }
    }
}

void line_lock_init(void) {
    line_lock_delay_init();

    // Drive line lock at 1 kHz with 50% duty for bring-up.
    line_lock_set_pwm(0.5f);

    osThreadNew(line_lock_task, NULL, &lineLockTask_attributes);
}

void line_lock_set_pwm(float percentage) {
    line_lock_on_time_us =
        (uint32_t)(clamp_pwm(percentage) * (float)LINE_LOCK_PWM_PERIOD_US);
}
