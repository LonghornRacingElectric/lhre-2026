#ifndef PDU_FIRMWARE_SPI_ADC_H
#define PDU_FIRMWARE_SPI_ADC_H

#include <stdint.h>

typedef struct AdcVoltages {
  float dcBusVoltage;
  float dcBusCurrent;

  float brakeLightCurrent;
  float glvCurrent;
  float radiatorFansCurrent;
  float batteryFansCurrent;
  float pumpCurrent;
  float accessoryCurrent;
  float shutdownCurrent;

  float sus1;
  float sus2;

  float ambientTemp;
  float loopTemp1;
  float loopTemp2;

  float extraAnalog1;
  float extraAnalog2;
} AdcVoltages;


static void spiAdc_csLow();

static void spiAdc_csHigh();

static float spiAdc_convert(uint8_t channel);

void spiAdc_init();

void spiAdc_getVoltages(AdcVoltages& adcVoltages);

#endif //PDU_FIRMWARE_SPI_ADC_H
