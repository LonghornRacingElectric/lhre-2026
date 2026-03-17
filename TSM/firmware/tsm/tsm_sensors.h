#include "cmsis_os2.h"
#include "main.h"
#include <math.h>
#include <stdint.h>

#define ADC_MAX 4095.0f
#define VREF 3.3f
#define R_FIXED 10000.0f
#define THERM_R0 10000.0f
#define THERM_BETA 3977.0f
#define TEMP_REF_K 298.15f

#define DS18B20_PORT GPIOA
#define DS18B20_PIN GPIO_PIN_3

float thermistor_adc_to_temp(uint16_t adc);
float ds18b20_read_temp(void);
uint16_t read_therm_adc(ADC_HandleTypeDef *hadc, uint32_t channel);