#include "spi_adc.h"

#include "main.h"
#include "spi.h"

static uint8_t buffer[32];

static void spiAdc_csLow() {
  HAL_GPIO_WritePin(SPI_CS_ADC_GPIO_Port, SPI_CS_ADC_Pin, GPIO_PIN_RESET);
}

static void spiAdc_csHigh() {
  HAL_GPIO_WritePin(SPI_CS_ADC_GPIO_Port, SPI_CS_ADC_Pin, GPIO_PIN_SET);
}

static float spiAdc_convert(uint8_t channel) {
  buffer[0] = 0b10000110 | (channel << 3);

  spiAdc_csLow();
  HAL_SPI_Transmit(&hspi2, buffer, 1, 1000);
  HAL_SPI_Receive(&hspi2, buffer, 2, 1000);
  spiAdc_csHigh();

  uint8_t x = ((buffer[0] << 4) & 0xF0) | ((buffer[1] >> 4) & 0x0F);
  return static_cast<float>(x) / 255.0f * 3.3f;
}

void spiAdc_init() {
  HAL_Delay(200);
  buffer[0] = 0b01111000;
  spiAdc_csLow();
  HAL_SPI_Transmit(&hspi2, buffer, 1, 1000);
  spiAdc_csHigh();
}

void spiAdc_getVoltages(AdcVoltages& adcVoltages) {
  adcVoltages.extraAnalog1 = spiAdc_convert(0);
  adcVoltages.sus2 = spiAdc_convert(1);
  adcVoltages.extraAnalog2 = spiAdc_convert(2);
  adcVoltages.dcBusVoltage = spiAdc_convert(3);
  adcVoltages.dcBusCurrent = spiAdc_convert(4);
  adcVoltages.shutdownCurrent = spiAdc_convert(5);
  adcVoltages.pumpCurrent = spiAdc_convert(6);
  adcVoltages.accessoryCurrent = spiAdc_convert(7);
  adcVoltages.batteryFansCurrent = spiAdc_convert(8);
  adcVoltages.brakeLightCurrent = spiAdc_convert(9);
  adcVoltages.glvCurrent = spiAdc_convert(10);
  adcVoltages.radiatorFansCurrent = spiAdc_convert(11);
  adcVoltages.sus1 = spiAdc_convert(12);
  adcVoltages.ambientTemp = spiAdc_convert(13);
  adcVoltages.loopTemp2 = spiAdc_convert(14);
  adcVoltages.loopTemp1 = spiAdc_convert(15);
}
