#ifndef __SUSPOT_H
#define __SUSPOT_H

#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_adc.h"
#include <stdint.h>

/**
 * @brief  Read suspension potentiometer value from a given ADC channel
 * @param  hadc    Pointer to ADC handle (e.g., &hadc1)
 * @param  channel ADC channel (e.g., ADC_CHANNEL_1)
 * @retval uint32_t ADC converted value
 */
uint32_t susPotGetVal(ADC_HandleTypeDef *hadc, uint32_t channel);


#endif /* __SUSPOT_H */