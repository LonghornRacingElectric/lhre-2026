#ifndef LVBMS_TEMPERATURE_CURRENT_H
#define LVBMS_TEMPERATURE_CURRENT_H

#include "stm32g4xx_hal.h"

void ReadTempAndCurrent(float *temperatures, float *current, ADC_HandleTypeDef *hadc1, ADC_HandleTypeDef *hadc2, ADC_HandleTypeDef *hadc3);

#endif