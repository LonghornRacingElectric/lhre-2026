#ifndef IMU_H
#define IMU_H

#include "stm32g4xx_hal.h"
#include <stdint.h>

typedef struct {
    float accel_x;
    float accel_y;
    float accel_z;
} imu_data_t;

/* Acceleration is in m/s^2. Failed reads leave the caller's sample unchanged. */
enum { IMU_OK = 0, IMU_ERROR_SPI = -1, IMU_ERROR_ID = -2 };
int  IMU_Init(SPI_HandleTypeDef *hspi);

int  IMU_Read(imu_data_t *out);

#endif
