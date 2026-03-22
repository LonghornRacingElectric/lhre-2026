#include "tsm_sensors.h"
#include "adc.h"
#include "cmsis_os2.h"
#include "gpio.h"
#include <math.h>
#include <stdio.h>

#define TEMP_ERR -127.0f   // disconnected/floating
#define TEMP_SHORT -126.0f // pulled to GND / shorted

static void delay_us(uint32_t us) {
  uint32_t start = DWT->CYCCNT;
  uint32_t ticks = us * (SystemCoreClock / 1000000);
  while ((DWT->CYCCNT - start) < ticks)
    ;
}

static inline void ds18b20_low(void) {
  HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_RESET);
}
static inline void ds18b20_release(void) {
  HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_SET);
}
static inline uint8_t ds18b20_read_pin(void) {
  return HAL_GPIO_ReadPin(DS18B20_PORT, DS18B20_PIN);
}

static uint8_t ds18b20_reset(void) {
  ds18b20_low();
  delay_us(480);
  ds18b20_release();
  delay_us(70);
  uint8_t presence = !ds18b20_read_pin();
  delay_us(410);
  return presence;
}

static void ds18b20_write_bit(uint8_t bit) {
  ds18b20_low();
  if (bit) {
    delay_us(1);
    ds18b20_release();
    delay_us(60);
  } else {
    delay_us(60);
    ds18b20_release();
    delay_us(1);
  }
}

static uint8_t ds18b20_read_bit(void) {
  ds18b20_low();
  delay_us(1);
  ds18b20_release();
  delay_us(10);
  uint8_t bit = ds18b20_read_pin();
  delay_us(55);
  return bit;
}

static uint8_t ds18b20_read_byte(void) {
  uint8_t value = 0;
  for (int i = 0; i < 8; i++)
    value |= (ds18b20_read_bit() << i);
  return value;
}

float ds18b20_read_temp(void) {
  if (!ds18b20_reset())
    return TEMP_ERR;

  for (int i = 0; i < 8; i++)
    ds18b20_write_bit((0xCC >> i) & 1);
  for (int i = 0; i < 8; i++)
    ds18b20_write_bit((0x44 >> i) & 1);
  osDelay(750);
  ds18b20_reset();
  for (int i = 0; i < 8; i++)
    ds18b20_write_bit((0xCC >> i) & 1);
  for (int i = 0; i < 8; i++)
    ds18b20_write_bit((0xBE >> i) & 1);

  uint8_t l = ds18b20_read_byte();
  uint8_t h = ds18b20_read_byte();
  int16_t raw = (h << 8) | l;

  float temp = raw / 16.0f;

  if (temp == 85.0f) // power-on default, likely means sensor is disconnected
    return TEMP_ERR;

  return temp;
}

float thermistor_adc_to_temp(uint16_t adc) {
  if (adc < ADC_MAX * 0.01)
    return TEMP_SHORT;
  if (adc > ADC_MAX * 0.97)
    return TEMP_ERR;
  float v = ((float)adc / ADC_MAX) * VREF;
  float r_therm = R_FIXED * (v / (VREF - v));
  float temp_k = 1.0f / ((1.0f / TEMP_REF_K) +
                         (1.0f / THERM_BETA) * logf(r_therm / THERM_R0));
  return temp_k - 273.15f;
}

uint16_t read_therm_adc(ADC_HandleTypeDef *hadc, uint32_t channel) {
  ADC_ChannelConfTypeDef sConfig = {0};
  sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.Channel = channel;

  HAL_ADC_ConfigChannel(hadc, &sConfig);
  HAL_ADC_Start(hadc);
  HAL_ADC_PollForConversion(hadc, 10);
  uint16_t val = HAL_ADC_GetValue(hadc);
  HAL_ADC_Stop(hadc);
  return val;
}

const char *temp_to_str(float temp, char *buf, size_t len) {
  if (temp == TEMP_ERR) {
    return "ERR";
  }

  if (temp == TEMP_SHORT) {
    return "SHORT";
  }
  snprintf(buf, len, "%.1f", temp);
  return buf;
}