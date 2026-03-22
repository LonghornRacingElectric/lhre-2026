#include "lvbms_temperature_current.h"
#include <math.h>

#define VCC 3.0f              // Supply voltage
#define R_FIXED 10000.0f      // Fixed resistor value (10kΩ)
#define R0 10000.0f           // Thermistor resistance at T0 (10kΩ at 25°C)
#define T0 298.15f            // Reference temperature (25°C in Kelvin)
#define BETA 3435.0f          // Beta coefficient for 103AT-4-10228 10k NTC thermistor

float ConvertToCelsius(uint32_t adcValue) {
    if (adcValue == 0 || adcValue >= 4095) {
        return NAN;  // Invalid ADC reading
    }

    float voltage = (adcValue / 4095.0f) * 3.3f;
    float r_thermistor = R_FIXED * voltage / (VCC - voltage);
    float steinhart = (1.0f / T0) + ((1.0f / BETA) * logf(r_thermistor / R0));
    float temp_k = 1.0f / steinhart;  // Temperature in Kelvin
    float temperature = temp_k - 273.15f;  // Convert to Celsius

    return temperature;
}

float ConvertToAmps(uint32_t adcValue) {
    float voltage = (adcValue / 4095.0f) * 3.3f;
    float current = voltage / 0.001f;  // Convert voltage to current using the shunt resistor value (0.001Ω)

    return current;
}

void ReadTempAndCurrent(float *temperatures, float *current, ADC_HandleTypeDef *hadc1, ADC_HandleTypeDef *hadc2, ADC_HandleTypeDef *hadc3) {
    uint32_t adcValue = 0;
    HAL_ADC_Start(hadc1);
    if (HAL_ADC_PollForConversion(hadc1, 100) == HAL_OK) {
        adcValue = HAL_ADC_GetValue(hadc1);
    }
    HAL_ADC_Stop(hadc1);
    temperatures[1] = ConvertToCelsius(adcValue);

    adcValue = 0;
    HAL_ADC_Start(hadc2);
    if (HAL_ADC_PollForConversion(hadc2, 100) == HAL_OK) {
        adcValue = HAL_ADC_GetValue(hadc2);
    }
    HAL_ADC_Stop(hadc2);
    temperatures[0] = ConvertToCelsius(adcValue);

    adcValue = 0;
    uint32_t currentValue = 0;
    HAL_ADC_Start(hadc3);
    if (HAL_ADC_PollForConversion(hadc3, 100) == HAL_OK) {
        adcValue = HAL_ADC_GetValue(hadc3);
        if (HAL_ADC_PollForConversion(hadc3, 100) == HAL_OK) {
            currentValue = HAL_ADC_GetValue(hadc3);
        }
    }
    HAL_ADC_Stop(hadc3);
    temperatures[2] = ConvertToCelsius(adcValue);
    *current = ConvertToAmps(currentValue);
}