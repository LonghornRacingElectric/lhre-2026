#include "line_lock.h"

#include "main.h"

/**
 * Line Lock timer-backed software PWM declarations
 */

#define LINE_LOCK_PWM_FREQUENCY_HZ 10000U
#define LINE_LOCK_TOGGLE_FREQUENCY_HZ (LINE_LOCK_PWM_FREQUENCY_HZ * 2U)
#define LINE_LOCK_TIMER_COUNTER_HZ 1000000U
#define LINE_LOCK_TIMER_IRQ_PRIORITY 6U

static uint32_t line_lock_tim6_clock_hz(void) {
    RCC_ClkInitTypeDef clk_config = {0};
    uint32_t flash_latency = 0;

    HAL_RCC_GetClockConfig(&clk_config, &flash_latency);
    if (clk_config.APB1CLKDivider == RCC_HCLK_DIV1) {
        return HAL_RCC_GetPCLK1Freq();
    }

    return HAL_RCC_GetPCLK1Freq() * 2U;
}

static uint32_t line_lock_prescaler(void) {
    uint32_t divider = line_lock_tim6_clock_hz() / LINE_LOCK_TIMER_COUNTER_HZ;

    if (divider == 0U) {
        return 0U;
    }

    return divider - 1U;
}

static uint32_t line_lock_period(void) {
    return (LINE_LOCK_TIMER_COUNTER_HZ / LINE_LOCK_TOGGLE_FREQUENCY_HZ) - 1U;
}

void line_lock_init(void) {
    HAL_GPIO_WritePin(Line_Lock_EN_GPIO_Port, Line_Lock_EN_Pin, GPIO_PIN_SET);

    __HAL_RCC_TIM6_CLK_ENABLE();

    TIM6->CR1 = 0U;
    TIM6->PSC = line_lock_prescaler();
    TIM6->ARR = line_lock_period();
    TIM6->CNT = 0U;
    TIM6->EGR = TIM_EGR_UG;
    TIM6->SR = 0U;
    TIM6->DIER = TIM_DIER_UIE;

    HAL_NVIC_SetPriority(TIM6_DAC_IRQn, LINE_LOCK_TIMER_IRQ_PRIORITY, 0U);
    HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);

    TIM6->CR1 = TIM_CR1_CEN;
}

static void line_lock_irq_handler(void) {
    if ((TIM6->SR & TIM_SR_UIF) == 0U) {
        return;
    }

    TIM6->SR &= ~TIM_SR_UIF;
    HAL_GPIO_TogglePin(Line_Lock_EN_GPIO_Port, Line_Lock_EN_Pin);
}

void TIM6_DAC_IRQHandler(void) {
    line_lock_irq_handler();
}
