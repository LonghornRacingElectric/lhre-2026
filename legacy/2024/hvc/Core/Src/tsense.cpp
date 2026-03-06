//
// Created by rolandwang on 11/12/2023.
//

#include "tsense.h"
#include "spi.h"

void tsenseInit() {
  // TODO implement
}

void tsensePeriodic() {
  HAL_GPIO_WritePin(SPI_CS_TEMP_GPIO_Port, SPI_CS_TEMP_Pin, GPIO_PIN_RESET);
  HAL_SPI_Receive(&hspi1, thermistorData, 1, SPI_TIMEOUT);
  HAL_GPIO_WritePin(SPI_CS_TEMP_GPIO_Port, SPI_CS_TEMP_Pin, GPIO_PIN_SET);
}

float getAmbientTemp() {
  rawTempData = ((thermistorData[0] << 8) || thermistorData[1]);

  if (rawTempData & 0x8000u) { // If MSB is 1 -> negative temperature
    ambientTemp = -256.0f + (float) ((rawTempData >> 3) & 0x0FFF) / 16.0f;
  } else {
    ambientTemp = (float) ((rawTempData >> 3) & 0x0FFF) / 16.0f;
  }

  return ambientTemp;
}