#include "pdu.h"
#include "LonghornLib/angel_can.h"
#include "LonghornLib/faults.h"
#include "timeouts.h"

static CanInbox lvbattStatusInbox;
static CanInbox thermalStatusInbox;
static CanInbox lvCurrents1Inbox;
static CanInbox lvCurrents2Inbox;

static CanOutbox brakeLightOutbox;
static CanOutbox buzzerOutbox;
static CanOutbox coolingOutbox;

void pdu_init() {
  can_addInbox(PDU_VCU_LVBAT, &lvbattStatusInbox, PDU_TIMEOUT_SLOW);
  can_addInbox(PDU_VCU_THERMAL, &thermalStatusInbox, PDU_TIMEOUT_SLOW);
  can_addInbox(PDU_VCU_LV_CURRENTS_1, &lvCurrents1Inbox, PDU_TIMEOUT_SLOW);
  can_addInbox(PDU_VCU_LV_CURRENTS_2, &lvCurrents2Inbox, PDU_TIMEOUT_SLOW);

  can_addOutbox(VCU_PDU_BRAKELIGHT, 0.01f, &brakeLightOutbox);
  can_addOutbox(VCU_PDU_BUZZER, 0.01f, &buzzerOutbox);
  can_addOutbox(VCU_PDU_COOLING, 0.1f, &coolingOutbox);
}

static void pdu_updateBrakeLight(float brightness) {
  brakeLightOutbox.dlc = 2;
  can_writeFloat(uint16_t, &brakeLightOutbox, 0, brightness, 0.0001f);
  brakeLightOutbox.isRecent = true;
}

static void pdu_updateBuzzer(BuzzerType buzzerType) {
  buzzerOutbox.dlc = 1;
  buzzerOutbox.data[0] = (uint8_t) buzzerType;
  buzzerOutbox.isRecent = true;
}

static void pdu_updateCoolingOutput(float radiatorFanRpmPercentage, float pumpPercentage) {
  coolingOutbox.dlc = 2;
  coolingOutbox.data[0] = (uint8_t) (radiatorFanRpmPercentage * 100.0f); // max will occur at 8300 rpm (100%), but higher will just be ignored
  coolingOutbox.data[1] = (uint8_t) (pumpPercentage * 100.0f);
  coolingOutbox.isRecent = true;
}

void pdu_periodic(PduStatus *status, VcuOutput *vcuOutput) {

  // At the moment, vcuCore only outputs a boolean for on and off in general.
  pdu_updateBrakeLight(vcuOutput->brakeLight);

  pdu_updateBuzzer(vcuOutput->r2dBuzzer ? BUZZER_BUZZ : BUZZER_SILENT);

  // At the moment, they are only set to max values by vcuCore
  // They are percentages, so they will be sent as floats between 0 and 1.
  pdu_updateCoolingOutput(vcuOutput->radiatorOutput, vcuOutput->pumpOutput);

  if (lvbattStatusInbox.isRecent) {
    lvbattStatusInbox.isRecent = false;
    status->lvVoltage = can_readFloat(int16_t, &lvbattStatusInbox, 0, 0.01f); // in 0.01V units, range is 0.00 - 28.00V
    status->lvSoC = can_readFloat(uint16_t, &lvbattStatusInbox, 2, 0.01f); // in 0.01% units, range is 0.00 - 100.00%
    status->lvCurrent = can_readFloat(int16_t, &lvbattStatusInbox, 4, 0.01f); // in 0.01A units, range is 0.00 - 100.00A
    status->isRecent = true;
  }
  if (thermalStatusInbox.isRecent) {
    thermalStatusInbox.isRecent = false;
    status->volumetricFlowRate = can_readFloat(uint8_t, &thermalStatusInbox, 0, 0.1f); // in 0.1L/min units, range is 0 - 25.5L/min
    status->waterTempMotor = can_readInt(int8_t, &thermalStatusInbox, 1); // in 0.1C units, range is -40.0 - 215.0C (max is 215C, but higher will just be ignored
    status->waterTempInverter = can_readInt(int8_t, &thermalStatusInbox, 2); // in 0.1C units, range is -40.0 - 215.0C (max is 215C, but higher will just be ignored
    status->waterTempRadiator = can_readInt(int8_t, &thermalStatusInbox, 3); // in 0.1C units, range is -40.0 - 215.0C (max is 215C, but higher will just be ignored
    status->radiatorFanRpm =
        can_readInt(uint16_t, &thermalStatusInbox, 4); // in rpm units, range is 0 - 8300rpm (max is 8300rpm, but higher will just be ignored)
    status->isRecent = true;
  }
  if(lvCurrents1Inbox.isRecent) {
    lvCurrents1Inbox.isRecent = false;
    status->treetrunk = can_readFloat(uint16_t, &lvCurrents1Inbox, 0, 0.01f);
    status->radfan = can_readFloat(uint16_t, &lvCurrents1Inbox, 2, 0.01f);
    status->battfan = can_readFloat(uint16_t, &lvCurrents1Inbox, 4, 0.01f);
    status->pump1 = can_readFloat(uint16_t, &lvCurrents1Inbox, 6, 0.01f);
    status->isRecent = true;
  }

  if(lvCurrents2Inbox.isRecent) {
    lvCurrents2Inbox.isRecent = false;
    status->pump2 = can_readFloat(uint16_t, &lvCurrents2Inbox, 0, 0.01f);
    status->glv = can_readFloat(uint16_t, &lvCurrents2Inbox, 2, 0.01f);
    status->shdn = can_readFloat(uint16_t, &lvCurrents2Inbox, 4, 0.01f);
    status->bl = can_readFloat(uint16_t, &lvCurrents2Inbox, 6, 0.01f);
    status->isRecent = true;
  }

  auto pdu_timeout = lvCurrents2Inbox.isTimeout || lvCurrents1Inbox.isTimeout ||
    thermalStatusInbox.isTimeout || lvbattStatusInbox.isTimeout;
  if(pdu_timeout) {
    FAULT_SET(&faultVector, FAULT_VCU_PDU);
  } else {
    FAULT_CLEAR(&faultVector, FAULT_VCU_PDU);
  }
}