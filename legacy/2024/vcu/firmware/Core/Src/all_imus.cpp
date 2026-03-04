#include "all_imus.h"
#include "LonghornLib/angel_can_ids.h"
#include "LonghornLib/angel_can.h"
#include "LonghornLib/faults.h"
#include "timeouts.h"

static CanInbox imu_hvcaccel_inbox;
static CanInbox imu_hvcgyro_inbox;
static CanInbox imu_pduaccel_inbox;
static CanInbox imu_pdugyro_inbox;
static CanInbox imu_unsSfr_inbox;
static CanInbox imu_unsSfl_inbox;
static CanInbox imu_unsSbr_inbox;
static CanInbox imu_unsSbl_inbox;

void allImus_init(SPI_HandleTypeDef *hspi_ptr) {
    imu_init(hspi_ptr);
    can_addInbox(HVC_VCU_IMU_ACCEL, &imu_hvcaccel_inbox, HVC_TIMEOUT_FAST);
    can_addInbox(HVC_VCU_IMU_GYRO, &imu_hvcgyro_inbox, HVC_TIMEOUT_FAST);
    can_addInbox(PDU_VCU_IMU_ACCEL, &imu_pduaccel_inbox, PDU_TIMEOUT_FAST);
    can_addInbox(PDU_VCU_IMU_GYRO, &imu_pdugyro_inbox, PDU_TIMEOUT_FAST);
    can_addInbox(UNSFR_VCU_IMU, &imu_unsSfr_inbox, UNS_TIMEOUT);
    can_addInbox(UNSFL_VCU_IMU, &imu_unsSfl_inbox, UNS_TIMEOUT);
    can_addInbox(UNSBR_VCU_IMU, &imu_unsSbr_inbox, UNS_TIMEOUT);
    can_addInbox(UNSBL_VCU_IMU, &imu_unsSbl_inbox, UNS_TIMEOUT);
}

static void externalImus_getAccels(xyz* accelHvc, xyz* accelPdu, xyz* accelFl, xyz* accelFr, xyz* accelBl, xyz* accelBr) {
  if(imu_hvcaccel_inbox.isRecent) {
    accelHvc->x = can_readFloat(int16_t, &imu_hvcaccel_inbox, 0, 0.01f);
    accelHvc->y = can_readFloat(int16_t, &imu_hvcaccel_inbox, 2, 0.01f);
    accelHvc->z = can_readFloat(int16_t, &imu_hvcaccel_inbox, 4, 0.01f);
    imu_hvcaccel_inbox.isRecent = false;
  }
  if(imu_hvcaccel_inbox.isTimeout) {
    FAULT_SET(&faultVector, FAULT_VCU_HVC_NOT_TELEM);
  }
  else {
    FAULT_CLEAR(&faultVector, FAULT_VCU_HVC_NOT_TELEM);
  }

  if(imu_pduaccel_inbox.isRecent) {
    accelPdu->x = can_readFloat(int16_t, &imu_pduaccel_inbox, 0, 0.01f);
    accelPdu->y = can_readFloat(int16_t, &imu_pduaccel_inbox, 2, 0.01f);
    accelPdu->z = can_readFloat(int16_t, &imu_pduaccel_inbox, 4, 0.01f);
    imu_pduaccel_inbox.isRecent = false;
  }
  if(imu_pduaccel_inbox.isTimeout) {
    FAULT_SET(&faultVector, FAULT_VCU_PDU);
  }
  else {
    FAULT_CLEAR(&faultVector, FAULT_VCU_PDU);
  }

  if(imu_unsSfl_inbox.isRecent) {
    accelFl->x = can_readFloat(int16_t, &imu_unsSfl_inbox, 0, 0.01f);
    accelFl->y = can_readFloat(int16_t, &imu_unsSfl_inbox, 2, 0.01f);
    accelFl->z = can_readFloat(int16_t, &imu_unsSfl_inbox, 4, 0.01f);
    imu_unsSfl_inbox.isRecent = false;
  }
  if(imu_unsSfl_inbox.isTimeout) {
    FAULT_SET(&faultVector, FAULT_VCU_UNS_FL);
  }
  else {
    FAULT_CLEAR(&faultVector, FAULT_VCU_UNS_FL);
  }

  if(imu_unsSfr_inbox.isRecent) {
    accelFr->x = can_readFloat(int16_t, &imu_unsSfr_inbox, 0, 0.01f);
    accelFr->y = can_readFloat(int16_t, &imu_unsSfr_inbox, 2, 0.01f);
    accelFr->z = can_readFloat(int16_t, &imu_unsSfr_inbox, 4, 0.01f);
    imu_unsSfr_inbox.isRecent = false;
  }
  if(imu_unsSfr_inbox.isTimeout) {
    FAULT_SET(&faultVector, FAULT_VCU_UNS_FR);
  }
  else {
    FAULT_CLEAR(&faultVector, FAULT_VCU_UNS_FR);
  }

  if(imu_unsSbl_inbox.isRecent) {
    accelBl->x = can_readFloat(int16_t, &imu_unsSbl_inbox, 0, 0.01f);
    accelBl->y = can_readFloat(int16_t, &imu_unsSbl_inbox, 2, 0.01f);
    accelBl->z = can_readFloat(int16_t, &imu_unsSbl_inbox, 4, 0.01f);
    imu_unsSbl_inbox.isRecent = false;
  }
  if(imu_unsSbl_inbox.isTimeout) {
    FAULT_SET(&faultVector, FAULT_VCU_UNS_BL);
  }
  else {
    FAULT_CLEAR(&faultVector, FAULT_VCU_UNS_BL);
  }

  if(imu_unsSbr_inbox.isRecent) {
    accelBr->x = can_readFloat(int16_t, &imu_unsSbr_inbox, 0, 0.01f);
    accelBr->y = can_readFloat(int16_t, &imu_unsSbr_inbox, 2, 0.01f);
    accelBr->z = can_readFloat(int16_t, &imu_unsSbr_inbox, 4, 0.01f);
    imu_unsSbr_inbox.isRecent = false;
  }
  if(imu_unsSbr_inbox.isTimeout) {
    FAULT_SET(&faultVector, FAULT_VCU_UNS_BR);
  }
  else {
    FAULT_CLEAR(&faultVector, FAULT_VCU_UNS_BR);
  }

}

static void externalImus_getGyros(xyz* gyroHvc, xyz* gyroPdu) {
  if(imu_hvcgyro_inbox.isRecent) {
    gyroHvc->x = can_readFloat(int16_t, &imu_hvcgyro_inbox, 0, 0.01f);
    gyroHvc->y = can_readFloat(int16_t, &imu_hvcgyro_inbox, 2, 0.01f);
    gyroHvc->z = can_readFloat(int16_t, &imu_hvcgyro_inbox, 4, 0.01f);
    imu_hvcgyro_inbox.isRecent = false;
  }
  if(imu_hvcgyro_inbox.isTimeout) {
    FAULT_SET(&faultVector, FAULT_VCU_HVC_NOT_TELEM);
  }
  else {
    FAULT_CLEAR(&faultVector, FAULT_VCU_HVC_NOT_TELEM);
  }

  if(imu_pdugyro_inbox.isRecent) {
    gyroPdu->x = can_readFloat(int16_t, &imu_pdugyro_inbox, 0, 0.01f);
    gyroPdu->y = can_readFloat(int16_t, &imu_pdugyro_inbox, 2, 0.01f);
    gyroPdu->z = can_readFloat(int16_t, &imu_pdugyro_inbox, 4, 0.01f);
    imu_pdugyro_inbox.isRecent = false;
  }
  if(imu_pdugyro_inbox.isTimeout) {
    FAULT_SET(&faultVector, FAULT_VCU_PDU);
  }
  else {
    FAULT_CLEAR(&faultVector, FAULT_VCU_PDU);
  }
}

void allImus_periodic(ImuData *imuData) {

  if(imu_isAccelReady())
    imu_getAccel(&imuData->accelVcu);
  if(imu_isGyroReady())
    imu_getGyro(&imuData->gyroVcu);

  externalImus_getAccels(&imuData->accelHvc, &imuData->accelPdu, &imuData->accelFl, &imuData->accelFr, &imuData->accelBl, &imuData->accelBr);
  externalImus_getGyros(&imuData->gyroHvc, &imuData->gyroPdu);
}