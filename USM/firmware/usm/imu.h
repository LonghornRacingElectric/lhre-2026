#ifndef IMU_H
#define IMU_H

#include "stm32g4xx_hal.h"
#include <stdint.h>

typedef struct {
    float accel_x;
    float accel_y;
    float accel_z;
} imu_data_t;

int  IMU_Init(SPI_HandleTypeDef *hspi);

int  IMU_Read(imu_data_t *out);

#endif
