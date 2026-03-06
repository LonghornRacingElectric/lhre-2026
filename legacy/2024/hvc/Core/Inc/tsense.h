//
// Created by rolandwang on 11/12/2023.
//

#ifndef HVC_FIRMWARE_2024_TSENSE_H
#define HVC_FIRMWARE_2024_TSENSE_H

#define SPI_TIMEOUT 1000 // Timeout

#include <cstdint>

static uint8_t thermistorData[2];
static uint16_t rawTempData = 0x0000;
static float ambientTemp = 0.0f;

void tsenseInit();

void tsensePeriodic();

float getAmbientTemp();

#endif //HVC_FIRMWARE_2024_TSENSE_H
