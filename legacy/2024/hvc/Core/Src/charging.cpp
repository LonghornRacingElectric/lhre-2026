#include "charging.h"

#include <string>

#include "fdcan.h"
#include "adc.h"
#include "gpio.h"
#include "LonghornLib/clock.h"
#include "soc_estimation.h"
#include "usb.h"

static bool chargerPluggedIn = false;
static float chargerMaxCurrent = 0;
static bool chargingDone = false;


void chargingInit() {
  writePilotCtrl(1);

  int error = HAL_FDCAN_Start(&hfdcan2);
  if (error != HAL_OK) {
    // TODO fault
  }
}

bool isChargerPluggedIn() {
  return chargerPluggedIn;
}

static void readProximity(float deltaTime) {
  static float holdTime = 0;

  // plugged in, trigger released = 1.03 V
  // plugged in, trigger pressed = 1.96 V
  // not plugged in = 2.94 V
  volatile float proxVoltage = getProximity();
  bool newState = proxVoltage < 1.5f;

  // debounce
  if (newState == chargerPluggedIn) {
    holdTime = 0;
  } else {
    holdTime += deltaTime;
    if (holdTime > 0.500f) {
      chargerPluggedIn = newState;
      holdTime = 0;
    }
  }
}

static void readControlPilot() {
  HAL_Delay(1000); // wait for J1772 to get plugged in all the way since the CP line is physically shorter

  float highCount = 0, lowCount = 0;

  float currPilotVoltage = getPilot(); // Samples ADC value from Pilot
  // Checks whether it's positive edge to start new sampling and end the previous sampling

  float startTime = clock_getTime();
  while (clock_getTime() - startTime < 1.0f) {
    // should see 0.73V at pin if control is in high part of duty cycle, 0 otherwise
    if (currPilotVoltage > 0.4f) {
      highCount++;
    } else {
      lowCount++;
    }
  }
  float dutyCycle = highCount / (highCount + lowCount) * 100.0f;

  if (dutyCycle > 85.0f && dutyCycle <= 100.0f) {
    chargerMaxCurrent = (dutyCycle - 64.0f) * 2.5f;
  } else if (dutyCycle >= 10.0f && dutyCycle <= 85.0f) {
    chargerMaxCurrent = dutyCycle * 0.6f;
  } else {
    // If duty cycle is between 0 and 10, or is invalid
    chargerMaxCurrent = 0.0f;
  }
}

static void sendElconCommand(float voltage, float current, bool enable) {
  // 0.5 second timer
  static float lastElconCommand = 0;
  if (clock_getTime() - lastElconCommand < 0.5f) {
    return;
  }
  lastElconCommand = clock_getTime();

  uint8_t data[8] = {0, 0, 0, 0, 0, 0, 0, 0};

  auto voltageInt = (uint16_t) (voltage * 10.0f);
  data[0] = voltageInt >> 8;
  data[1] = voltageInt & 0xFF;

  auto currentInt = (uint16_t) (current * 10.0f);
  data[2] = currentInt >> 8;
  data[3] = currentInt & 0xFF;

  // 0 is on, 1 is off
  data[4] = (enable) ? 0 : 1;


  static FDCAN_TxHeaderTypeDef TxHeader;
  TxHeader.Identifier = HVC_ELCON_CHARGE_COMMAND;
  TxHeader.IdType = FDCAN_EXTENDED_ID;
  TxHeader.TxFrameType = FDCAN_DATA_FRAME;
  TxHeader.DataLength = FDCAN_DLC_BYTES_8;
  TxHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  TxHeader.BitRateSwitch = FDCAN_BRS_OFF;
  TxHeader.FDFormat = FDCAN_CLASSIC_CAN;
  TxHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
  TxHeader.MessageMarker = 0;

  uint32_t error = HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan2, &TxHeader, data);
  if (error != HAL_OK) {
    // TODO fault
  }
}

static void sendDashMessage(bool plug, bool shutdown, bool energized, bool ams, bool imd,
                            float packVoltage, float soc, float timeRemaining) {
  // 0.01 second timer
//  plug = true;
  shutdown = true;
  energized = true;

  static float lastDashMessage = 0;
  if (clock_getTime() - lastDashMessage < 0.01f) {
    return;
  }
  lastDashMessage = clock_getTime();

  uint8_t data[8] = {0, 0, 0, 0, 0, 0, 0, 0};

  uint16_t flags = (plug << 4) | (shutdown << 3) | (energized << 2) | (ams << 1) | (imd << 0);
  *((uint16_t *) &data[0]) = flags;

  auto packVoltageInt = (uint16_t) (packVoltage / 0.01f);
  *((uint16_t *) &data[2]) = packVoltageInt;

  auto socEstimateInt = (uint16_t) (soc / 0.0001f);
  *((uint16_t *) &data[4]) = socEstimateInt;

  auto timeRemainingInt = (uint16_t) (timeRemaining);
  *((uint16_t *) &data[6]) = timeRemainingInt;

  static FDCAN_TxHeaderTypeDef TxHeader;
  TxHeader.Identifier = HVC_DASH_STATUS; // 0x420
  TxHeader.IdType = FDCAN_EXTENDED_ID;
  TxHeader.TxFrameType = FDCAN_DATA_FRAME;
  TxHeader.DataLength = FDCAN_DLC_BYTES_8;
  TxHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  TxHeader.BitRateSwitch = FDCAN_BRS_OFF;
  TxHeader.FDFormat = FDCAN_CLASSIC_CAN;
  TxHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
  TxHeader.MessageMarker = 0;

  uint32_t error = HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan2, &TxHeader, data);
  if (error != HAL_OK) {
    // TODO fault
  }
}

static void receiveElconStatus() {
  static FDCAN_RxHeaderTypeDef RxHeader;
  uint8_t data[8] = {0};
  uint32_t error = HAL_FDCAN_GetRxMessage(&hfdcan2, FDCAN_RX_FIFO0, &RxHeader, data);
  uint32_t id = RxHeader.Identifier;
  if (error != HAL_OK) {
    // TODO fault
    return;
  }

  if(id == ELCON_HVC_CHARGE_STATUS) {
    chargerPluggedIn = true;
  } else {
    return;
  }

  // we don't care about the data in this packet, that's the dash's job to display.
  // we'll read it anyway in case we wanna debug.

  volatile float voltage = (float) ((data[0] << 8) | data[1]) * 0.1f;
  volatile float current = (float) ((data[2] << 8) | data[3]) * 0.1f;
  volatile uint8_t stateVector = data[4];

  volatile bool elconHardwareFailure = stateVector & 0x01;
  volatile bool elconOverTemp = stateVector & 0x02;
  volatile bool elconIncorrectVoltage = stateVector & 0x04;
  volatile bool elconChargerOff = stateVector & 0x08; // can be triggered by battery unplugged
  volatile bool elconCommsFailure = stateVector & 0x10;

  if(voltage > MAX_CHARGE_VOLTAGE - 1.0f && current < 1.0f) {
    chargingDone = true;
  }

  // std::string space;
  // println(space);
  // std::string s = "charging current:";
  // println(s);
  // println(current);
  // println(space);
  // s = "charging done:";
  // println(s);
  // println(chargingDone);
  // println(space);

  stateVector++; // << good breakpoint
}


void chargingPeriodic(bool shutdown, bool energized, bool ams, bool imd, float packVoltage, float maxCellMargin, float deltaTime) {
  static bool wasChargerPluggedIn = false;
  static float socEstimate = 0;

  packVoltage = MAX_CHARGE_VOLTAGE; // changed for short-term BMS

  // TODO prox detection isn't working
//  readProximity(deltaTime);
  sendDashMessage(chargerPluggedIn, shutdown, energized, ams, imd,
                  packVoltage, socEstimate, 420.0f);
  // TODO SoC and time remaining estimation

  if (chargerPluggedIn > wasChargerPluggedIn) {
    wasChargerPluggedIn = true;
    socEstimate = getSoC(0);
    readControlPilot();
  } else if (chargerPluggedIn < wasChargerPluggedIn) {
    wasChargerPluggedIn = false;
  }

  // hard-coded current for our generator (240V)
  // float currentLimit = 9.5f;
  // current limit for ChargePoint (240V)
  float currentLimit = 8.0f;
  // current limit for 110V outlet
  // float currentLimit = 2.5f;

  float cellVoltageMarginBreak = 0.050f;
  /*
   * TODO logic that figures out based on chargerMaxCurrent if this is the 110V charger or a 220V one.
   * if it's the 110V one, we need to lower the charge current to like 3A to not destroy the pickle outlet.
   * Rylan said 110V one has a CP duty cycle of 25%, while the 220V one is 50%.
   */
  if(chargingDone) {
    sendElconCommand(MAX_CHARGE_VOLTAGE, 0, false);
  } else if(maxCellMargin < cellVoltageMarginBreak) {
    sendElconCommand(MAX_CHARGE_VOLTAGE, currentLimit * (maxCellMargin / cellVoltageMarginBreak), energized);
    if(maxCellMargin < 0.005f)
    {
      chargingDone = true;
    }
  } else {
    sendElconCommand(MAX_CHARGE_VOLTAGE, currentLimit, energized);
  }
  receiveElconStatus();

}
