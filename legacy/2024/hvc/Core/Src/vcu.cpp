
#include "vcu.h"
#include "LonghornLib/angel_can.h"
#include "isense.h"
#include "vsense.h"
#include "cells.h"
#include "LonghornLib/imu.h"
#include "fans.h"
#include "tsense.h"
#include "soc_estimation.h"
#include "spi.h"

static CanInbox parameterInbox;
static CanInbox allowBalanceInbox;
static CanOutbox packStatus;
static CanOutbox imuAccel;
static CanOutbox imuGyro;
static CanOutbox indicatorStatus;
static CanOutbox fanPWMs;
static CanOutbox ccsInfo;
static CanOutbox contactorStatus;

static xyz accelData;
static xyz gyroData;

void vcuInit() {
  can_addInbox(VCU_HVC_PARAMS, &parameterInbox, 1.0f);
  can_addInbox(VCU_HVC_ALLOW_BALANCE, &allowBalanceInbox, 1.0f);

  can_addOutbox(HVC_VCU_PACK_STATUS, 0.1f, &packStatus);
  packStatus.dlc = 8;
  // can_addOutbox(HVC_VCU_IMU_ACCEL, 0.003f, &imuAccel);
  imuAccel.dlc = 6;
  // can_addOutbox(HVC_VCU_IMU_GYRO, 0.003f, &imuGyro);
  imuGyro.dlc = 6;
  can_addOutbox(HVC_VCU_AMS_IMD, 0.1f, &indicatorStatus);
  indicatorStatus.dlc = 2;
//  can_addOutbox(HVC_VCU_FAN_RPM, 0.1f, &fanRPMs);
//  fanRPMs.dlc = 4;
//  can_addOutbox(HVC_VCU_CCS_INFO, 1.0f, &ccsInfo);
//  ccsInfo.dlc = 0;  // TODO
  can_addOutbox(HVC_VCU_CONTACTOR_STATUS, 0.1f, &contactorStatus);
  contactorStatus.dlc = 1;

  imu_init(&hspi1);
}

/**
 *  Get pack current, voltage, SoC, max temp -> store in data array (2 bytes each)
 *  Get imu accel and gyro data
 * */
void vcuPeriodic(bool amsIndicator, bool imdIndicator, int state, float deltaTime) {

  if (imu_isAccelReady())
    imu_getAccel(&accelData);
  if (imu_isGyroReady())
    imu_getGyro(&gyroData);

  // Battery Pack and IMU Data
  can_writeFloat(uint16_t, &packStatus, 0, getPackVoltageFromCells(), 0.01f);
  can_writeFloat(int16_t, &packStatus, 2, getPackCurrent(), 0.01f);
  can_writeFloat(uint16_t, &packStatus, 4, getSoC(deltaTime), 0.01f);
  can_writeFloat(uint8_t, &packStatus, 6, getMaxTemp(), 1.0f);
  can_writeFloat(uint8_t, &packStatus, 7, getAmbientTemp(), 1.0f);

  // can_writeFloat(int16_t, &imuAccel, 0, accelData.x, 0.01f);
  // can_writeFloat(int16_t, &imuAccel, 2, accelData.y, 0.01f);
  // can_writeFloat(int16_t, &imuAccel, 4, accelData.z, 0.01f);
  //
  // can_writeFloat(int16_t, &imuGyro, 0, gyroData.x, 0.01f);
  // can_writeFloat(int16_t, &imuGyro, 2, gyroData.y, 0.01f);
  // can_writeFloat(int16_t, &imuGyro, 4, gyroData.z, 0.01f);

  // can_writeFloat(int16_t, &fanPWMs, 0, (pwmDutyCycleMain * 100.0f), 1.0f);
  // can_writeFloat(int16_t, &fanPWMs, 2, (pwmDutyCycleUnique * 100.0f), 1.0f);

  // Indicator Status
  can_writeInt(uint8_t, &indicatorStatus, 0, amsIndicator);
  can_writeInt(uint8_t, &indicatorStatus, 1, imdIndicator);

  // CCS Info
  // TODO implement

  // Contactor Status
  can_writeInt(uint8_t, &contactorStatus, 0, state);

  // Check VCU->HVC Params Inbox
  if (parameterInbox.isRecent) {
    parameterInbox.isRecent = false;
    float underVoltage = can_readFloat(uint16_t, &parameterInbox, 0, 0.1f);
    float overVoltage = can_readFloat(uint16_t, &parameterInbox, 2, 0.1f);
    float underTemp = can_readFloat(uint16_t, &parameterInbox, 4, 1.0f);
    float overTemp = can_readFloat(uint16_t, &parameterInbox, 6, 1.0f);
    updateBmsLimits(underVoltage, overVoltage, underTemp, overTemp);
  }

  if(allowBalanceInbox.isRecent) {
    allowBalanceInbox.isRecent = false;
    carParked = can_readInt(uint8_t, &allowBalanceInbox, 0);
  } else if(allowBalanceInbox.isTimeout)
  {
    // carParked = true; // TODO false!!
    carParked = false;
  }
}
