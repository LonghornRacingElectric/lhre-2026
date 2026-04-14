#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_adc.h"
#include "adc.h"
#include "strain_gauge.h"
#include "main.h"

int32_t strainGaugeGetVal(ADC_HandleTypeDef *hadc, uint32_t channel) {
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.SamplingTime = ADC_SAMPLETIME_247CYCLES_5;
    sConfig.SingleDiff = ADC_DIFFERENTIAL_ENDED;  // Use differential mode for strain gauge
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset = 0;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.Channel = channel;

    HAL_ADC_ConfigChannel(hadc, &sConfig);
    HAL_ADC_Start(hadc);
    HAL_ADC_PollForConversion(hadc, HAL_MAX_DELAY);
    int32_t value = HAL_ADC_GetValue(hadc); // integer in case the outputs are swapped and the negative is higher than the positive. otherwise it might wrap around
    HAL_ADC_Stop(hadc);

    return value;
}