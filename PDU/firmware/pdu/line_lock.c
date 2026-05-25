#include "line_lock.h"

#include "adc.h"
#include "cmsis_os.h"
#include "main.h"

/**
 * Line Lock Software PWM Declarations
 */

#define LINE_LOCK_PWM_PERIOD_US 1000U
#define LINE_LOCK_TASK_DELAY_MS 100U
#define ADC_MAX_VAL ((1u << 12) - 1u)
#define ADC_BSE_SCALE_V 3.2837f

static volatile uint32_t line_lock_on_time_us = LINE_LOCK_PWM_PERIOD_US;
static volatile float line_lock_bse3_voltage_v = 0.0f;

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

static bool shutdown_sense_read(GPIO_TypeDef *port, uint16_t pin) {
    return HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_SET;
}

static float read_bse3_voltage(void) {
    uint32_t adc_value = 0U;

    HAL_ADC_Start(&hadc5);
    if (HAL_ADC_PollForConversion(&hadc5, 10) == HAL_OK) {
        adc_value = HAL_ADC_GetValue(&hadc5);
    }
    HAL_ADC_Stop(&hadc5);

    return ((float)adc_value * ADC_BSE_SCALE_V) / ADC_MAX_VAL;
}

static void line_lock_update_inputs(void) {
    line_lock_bse3_voltage_v = read_bse3_voltage();
}

static void line_lock_task(void *argument) {
    (void)argument;

    while (1) {
        uint32_t on_time_us = line_lock_on_time_us;

        line_lock_update_inputs();

        if (on_time_us >= LINE_LOCK_PWM_PERIOD_US) {
            HAL_GPIO_WritePin(Line_Lock_EN_GPIO_Port, Line_Lock_EN_Pin,
                              GPIO_PIN_SET);
            osDelay(LINE_LOCK_TASK_DELAY_MS);
        } else if (on_time_us == 0U) {
            HAL_GPIO_WritePin(Line_Lock_EN_GPIO_Port, Line_Lock_EN_Pin,
                              GPIO_PIN_RESET);
            osDelay(LINE_LOCK_TASK_DELAY_MS);
        } else {
            HAL_GPIO_WritePin(Line_Lock_EN_GPIO_Port, Line_Lock_EN_Pin,
                              GPIO_PIN_SET);
            delay_us(on_time_us);
            HAL_GPIO_WritePin(Line_Lock_EN_GPIO_Port, Line_Lock_EN_Pin,
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

float line_lock_bse3_voltage(void) { return line_lock_bse3_voltage_v; }

line_lock_shutdown_sense_t line_lock_shutdown_sense(void) {
    return (line_lock_shutdown_sense_t){
        .leg_11 =
            shutdown_sense_read(SDWN_11_Sense_GPIO_Port, SDWN_11_Sense_Pin),
        .leg_12 =
            shutdown_sense_read(SDWN_12_Sense_GPIO_Port, SDWN_12_Sense_Pin),
        .leg_13 =
            shutdown_sense_read(SDWN_13_Sense_GPIO_Port, SDWN_13_Sense_Pin),
        .leg_14 =
            shutdown_sense_read(SDWN_14_Sense_GPIO_Port, SDWN_14_Sense_Pin),
        .leg_15 =
            shutdown_sense_read(SDWN_15_Sense_GPIO_Port, SDWN_15_Sense_Pin),
    };
}

bool line_lock_shutdown_closed(void) {
    line_lock_shutdown_sense_t sense = line_lock_shutdown_sense();

    return sense.leg_11 && sense.leg_12 && sense.leg_13 && sense.leg_14 &&
           sense.leg_15;
}
