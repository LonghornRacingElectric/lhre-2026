#ifndef __STRAIN_GAUGE_H
#define __STRAIN_GAUGE_H

#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_adc.h"
#include <stdint.h>

/**
 * @brief  Read strain gauge value from a given ADC channel
 * @param  hadc    Pointer to ADC handle (e.g., &hadc1)
 * @param  channel ADC channel (e.g., ADC_CHANNEL_1)
 * @retval int32_t ADC converted value
 */
int32_t strainGaugeGetVal(ADC_HandleTypeDef *hadc, uint32_t channel);


#endif /* __STRAIN_GAUGE_H */