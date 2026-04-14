#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_adc.h"
#include "adc.h"
#include "suspot.h"
#include "main.h"

uint32_t susPotGetVal(ADC_HandleTypeDef *hadc, uint32_t channel) {
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.SamplingTime = ADC_SAMPLETIME_247CYCLES_5;
    sConfig.SingleDiff = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset = 0;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.Channel = channel;

    HAL_ADC_ConfigChannel(hadc, &sConfig);
    HAL_ADC_Start(hadc);
    HAL_ADC_PollForConversion(hadc, HAL_MAX_DELAY);
    uint32_t value = HAL_ADC_GetValue(hadc);
    HAL_ADC_Stop(hadc);

    return value;
}