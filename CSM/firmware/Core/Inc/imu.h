#ifndef IMU_H
#define IMU_H

#include "stm32g4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/*******************************************************************************
 * Data Structures
 ******************************************************************************/

typedef struct {
    float x;
    float y;
    float z;
} xyz_t;

typedef struct {
    xyz_t accel;  // m/s^2
    xyz_t gyro;   // rad/s
} imu_data_t;

/*******************************************************************************
 * Public Functions
 ******************************************************************************/

/**
 * @brief  Initializes the IMU over SPI and runs gyro bias calibration.
 * @param  hspi  Pointer to SPI handle (pass &hspi1)
 * @retval 0 on success, non-zero on failure (returns WHO_AM_I value if wrong)
 */
int IMU_Init(SPI_HandleTypeDef *hspi);

/**
 * @brief  Calibrates gyroscope bias at startup.
 * @note   Call while car is stationary. Accel is not calibrated since
 *         car may not be level at startup.
 */
void IMU_Calibrate(void);

/**
 * @brief  Checks if new accelerometer data is available.
 * @retval true if ready
 */
bool IMU_AccelStatus(void);

/**
 * @brief  Checks if new gyroscope data is available.
 * @retval true if ready
 */
bool IMU_GyroStatus(void);

/**
 * @brief  Reads accelerometer data with gyro bias removed.
 * @param  vec  Pointer to xyz_t to store result in m/s^2
 */
void IMU_GetAccel(xyz_t *vec);

/**
 * @brief  Reads gyroscope data with bias removed.
 * @param  vec  Pointer to xyz_t to store result in rad/s
 */
void IMU_GetGyro(xyz_t *vec);

/**
 * @brief  Reads both accel and gyro in one call.
 * @param  out  Pointer to imu_data_t to store result
 */
void IMU_GetData(imu_data_t *out);

uint8_t imu_read(uint8_t reg);

#endif /* IMU_H */