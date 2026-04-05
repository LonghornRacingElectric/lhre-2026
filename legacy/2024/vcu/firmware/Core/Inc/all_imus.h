#ifndef VCU_FIRMWARE_2024_ALL_IMUS_H
#define VCU_FIRMWARE_2024_ALL_IMUS_H

#include "LonghornLib/imu.h"


typedef struct ImuData {
  xyz accelVcu;
  xyz accelHvc;
  xyz accelPdu;
  xyz accelFl;
  xyz accelFr;
  xyz accelBl;
  xyz accelBr;
  xyz gyroVcu;
  xyz gyroHvc;
  xyz gyroPdu;
} ImuData;

/**
 * Initialize by subscribing to certain CAN IDs for example.
 */
void allImus_init(SPI_HandleTypeDef* hspi_ptr);

/**
 * Get latest IMU data from both local and external IMUs.
 */
void allImus_periodic(ImuData *imuData);

#endif //VCU_FIRMWARE_2024_ALL_IMUS_H
