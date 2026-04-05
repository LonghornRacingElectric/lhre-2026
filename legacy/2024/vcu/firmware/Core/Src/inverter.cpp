#include <queue>
#include "inverter.h"
#include "LonghornLib/faults.h"
#include "LonghornLib/angel_can.h"
#include "LonghornLib/angel_can_ids.h"
#include "timeouts.h"
#include "usb.h"
#include "LonghornLib/clock.h"

using namespace std;


static CanInbox voltageInbox;
static CanInbox currentInbox;
static CanInbox motorTempInbox;
static CanInbox inverterTempInbox;
static CanInbox motorPosInbox;
static CanInbox inverterStateInbox;
static CanInbox inverterFaultInbox;
static CanInbox torqueInfoInbox;
static CanInbox highSpeedInbox;

static CanOutbox torqueCommandOutbox;

void inverter_init() {
  can_addInbox(INV_VOLTAGE, &voltageInbox, INV_TIMEOUT_FAST);
  can_addInbox(INV_CURRENT, &currentInbox, INV_TIMEOUT_FAST);
  can_addInbox(INV_TEMP3_DATA, &motorTempInbox, INV_TIMEOUT_SLOW);
  can_addInbox(INV_TEMP1_DATA, &inverterTempInbox, INV_TIMEOUT_SLOW);
  can_addInbox(INV_MOTOR_POSITIONS, &motorPosInbox, INV_TIMEOUT_FAST);
  can_addInbox(INV_STATE_CODES, &inverterStateInbox, INV_TIMEOUT_FAST);
  can_addInbox(INV_FAULT_CODES, &inverterFaultInbox, INV_TIMEOUT_FAST);
  can_addInbox(INV_TORQUE_TIMER, &torqueInfoInbox, INV_TIMEOUT_FAST);
  //can_addInbox(INV_HIGH_SPEED_MSG, &highSpeedInbox, INV_TIMEOUT_VERYFAST);

  can_addOutbox(VCU_INV_COMMAND, 0.003f, &torqueCommandOutbox);


//  inverter_writeParameter(147, 1000); // CAN bit rate

//  inverter_writeParameter(148, 0x1CE5); // message selection
//  inverter_writeParameter(168, 10); // torque ramp, use 100 loaded
//  inverter_writeParameter(111, 6900); // motor over-speed fault RPM
//  inverter_writeParameter(128, 6500); // max RPM
//  inverter_writeParameter(127, 4000); // field weakening start RPM ("break speed")
//  inverter_writeParameter(169, 5100); // speed rate limit RPM
//  inverter_writeParameter(129, 2300); // torque limit
//  inverter_writeParameter(100, 4530); // Q axis current limit
//  inverter_writeParameter(101, 900); // D axis current limit
//  inverter_writeParameter(164, 100); // P gain
//  inverter_writeParameter(165, 360); // I gain
//  inverter_writeParameter(166, 0); // D gain
//  inverter_writeParameter(167, 0); // low-pass filter gain
//  inverter_writeParameter(187, 0); // shudder compensation
//  inverter_writeParameter(241, 12); // default PWM
//  inverter_writeParameter(246, 12); // min PWM
//  inverter_writeParameter(247, 12); // max PWM
//  inverter_writeParameter(245, 12); // stall PWM
//  inverter_writeParameter(250, 0); // nominal PWM mode with stall region

  // 246 = 6 = 6 kHz min PWM
  // 247 = 17 = 17 kHz max PWM
  // 245 = 6 = 6 kHz stall PWM
  // 250 = 1 = continuously variable with stall region
}

static void inverter_getStatus(InverterStatus *status) {

  if (inverterTempInbox.isRecent) {
    auto temp = can_readFloat(int16_t, &inverterTempInbox, 0, 0.1f) +
                can_readFloat(int16_t, &inverterTempInbox, 2, 0.1f) +
                can_readFloat(int16_t, &inverterTempInbox, 4, 0.1f);
    status->inverterTemp = temp / 3.0f;
    inverterTempInbox.isRecent = false;
    status->isRecent = true;
  }

  if (motorTempInbox.isRecent) {
    status->motorTemp = can_readFloat(int16_t, &motorTempInbox, 4, 0.1f);
    motorTempInbox.isRecent = false;
    status->isRecent = true;
  }

  if (motorPosInbox.isRecent) {
    status->motorAngle = can_readFloat(int16_t, &motorPosInbox, 0, 0.1f);
    status->rpm = can_readInt(int16_t, &motorPosInbox, 2); //Out of all of these, idk why this isnt shifted
    status->inverterFrequency = can_readFloat(int16_t, &motorPosInbox, 4, 0.1f);
    status->resolverAngle = can_readFloat(int16_t, &motorPosInbox, 6, 0.1f);
    motorPosInbox.isRecent = false;
    status->isRecent = true;
  }

  if (voltageInbox.isRecent) {
    status->voltage = can_readFloat(int16_t, &voltageInbox, 0, 0.1f);
    status->outputVoltage = can_readFloat(int16_t, &voltageInbox, 2, 0.1f);
    status->ABVoltage = can_readFloat(int16_t, &voltageInbox, 4, 0.1f);
    status->BCVoltage = can_readFloat(int16_t, &voltageInbox, 6, 0.1f);
    voltageInbox.isRecent = false;
    status->isRecent = true;
  }

  if (currentInbox.isRecent) {
    status->phaseACurrent = can_readFloat(int16_t, &currentInbox, 0, 0.1f);
    status->phaseBCurrent = can_readFloat(int16_t, &currentInbox, 2, 0.1f);
    status->phaseCCurrent = can_readFloat(int16_t, &currentInbox, 4, 0.1f);
    status->current = can_readFloat(int16_t, &currentInbox, 6, 0.1f);
    currentInbox.isRecent = false;
    status->isRecent = true;
  }

  if (inverterStateInbox.isRecent) {
    status->stateVector = can_readInt(uint64_t, &inverterStateInbox, 0);
    inverterStateInbox.isRecent = false;
    status->isRecent = true;
  }

  if (inverterFaultInbox.isRecent) {
    status->faultVector = can_readInt(uint64_t, &inverterFaultInbox, 0);
    inverterFaultInbox.isRecent = false;
    status->isRecent = true;
    volatile bool resolverDisconnected = status->faultVector & 0x4000000000000000;
    volatile int x = 0;
    if(status->faultVector) {
      x++;
    }
  }

  if (torqueInfoInbox.isRecent) {
    status->torqueCommand = can_readFloat(int16_t, &torqueInfoInbox, 0, 0.1f);
    status->torqueActual = can_readFloat(int16_t, &torqueInfoInbox, 2, 0.1f);
    torqueInfoInbox.isRecent = false;
    status->isRecent = true;
  }

  if(torqueInfoInbox.isTimeout || inverterFaultInbox.isTimeout ||
    inverterStateInbox.isTimeout || currentInbox.isTimeout ||
    voltageInbox.isTimeout || motorPosInbox.isTimeout ||
    motorTempInbox.isTimeout || inverterTempInbox.isTimeout) {
    FAULT_SET(&faultVector, FAULT_VCU_INV);
  }
  else {
    FAULT_CLEAR(&faultVector, FAULT_VCU_INV);
  }
}


static void inverter_updateTorqueCommand(float torque, float rpm, bool enable_inverter) {
  can_writeFloat(int16_t, &torqueCommandOutbox, 0, torque, 0.1f);
  can_writeFloat(int16_t, &torqueCommandOutbox, 2, rpm, 1.0f);
  torqueCommandOutbox.data[4] = 1; // direction bool
  torqueCommandOutbox.data[5] = (uint8_t) enable_inverter; // Enable
  torqueCommandOutbox.data[6] = 0;
  torqueCommandOutbox.data[7] = 0;
  torqueCommandOutbox.dlc = 8;
  torqueCommandOutbox.isRecent = true;
}

void inverter_periodic(InverterStatus *status, VcuOutput* vcuCoreOutput, float deltaTime) {
  inverter_getStatus(status);
  inverter_updateTorqueCommand(vcuCoreOutput->inverterTorqueRequest, 0, vcuCoreOutput->enableInverter);
}