//
// Created by rolan on 4/6/2025.
//

#include "cells.h"
#include "night_can.h"
#include "hvc_can.h"
#include "timer.h"
#include "state_machine.h"

float PACK_OVER_VOLTAGE = 546.0f;
float PACK_UNDER_VOLTAGE = 390.0f;
float CELL_OVER_VOLTAGE = 4.2f;
float CELL_UNDER_VOLTAGE = 3.00f;
float OVER_TEMP = 60.0f;
float UNDER_TEMP = 0.0f;
bool carParked = false;

static ADBMS6830_Command_t CMD_RDCs[9] = {
  CMD_RDFCA, CMD_RDFCB, CMD_RDFCC, CMD_RDFCD, CMD_RDFCE,
  CMD_RDAUXA, CMD_RDAUXB, CMD_RDAUXC, CMD_RDAUXD
};

static NightCANPacket cellVoltages[35];
static NightCANPacket cellTemps[23];
static uint32_t responsiveChips = 0;
static uint32_t completeScanResponsiveChips = 0;
static bool bmbReadOk[NUM_BMS_ICS];
static bool bmbReadOkThisScan[NUM_BMS_ICS];

/**
 * Initializes CAN outboxes to send voltage and temp data
 **/
void cells_init() {
  setDeadCells();
  setDeadThermistors();

  for(int i = 0; i < ARR_LEN(cellVoltages); i++) {
    cellVoltages[i] = CAN_create_packet(0x370 + i, 1.0f, 8);
    CAN_AddTxPacket(&can1, &cellVoltages[i]);
  }
  for(int i = 0; i < ARR_LEN(cellTemps); i++) {
    cellTemps[i] = CAN_create_packet(0x470 + i, 1.0f, 8);
    CAN_AddTxPacket(&can1, &cellTemps[i]);
  }
}

static int cmd_ID = -1;    // Used to track and send ADBMS cmds
static uint16_t value = 0; // Temporary variable
void cells_periodic(int state) {

  adbms6830_wakeup();

  if (cmd_ID == -1)
  {
    adbms6830_wrcfga();
    adbms6830_wrcfgb(false, balanceCommands);
  }
  //usb_printf("cmdID: %d\n", cmd_ID);
  if (cmd_ID == 0)
  {
    for (int i = 0; i < NUM_BMS_ICS; i++) {
      bmbReadOkThisScan[i] = true;
    }
    adbms6830_adcv();
    HAL_Delay(10);
    adbms6830_wakeup();
    adbms6830_wrcfgb(true, balanceCommands);
  }
  else if (cmd_ID == 5)
  {
    adbms6830_adax();
    responsiveChips = adbms6830_cmd_read(CMD_RDSTATB, rawData);
    for (int j = 0; j < NUM_BMS_ICS; j++) {
      bmbReadOkThisScan[j] &= adbms6830_is_ic_responsive(j);
    }
    for (int j = 0; j < NUM_BMS_ICS; j++)
    {
      if (!adbms6830_is_ic_responsive(j)) continue;
      volatile float vd = convertVoltage((rawData[j * 6 + 1] << 8) | rawData[j * 6]);
      volatile float va = convertVoltage((rawData[j * 6 + 3] << 8) | rawData[j * 6 + 2]);
      volatile float vres = convertVoltage((rawData[j * 6 + 5] << 8) | rawData[j * 6 + 4]);
      statVreg[j] = va;
    }
  }

  // ADBMS read cmd 0-4 for voltages
  if (cmd_ID >= 0 && cmd_ID <= 4)
  {
    responsiveChips = adbms6830_cmd_read(CMD_RDCs[cmd_ID], rawData);
    for (int j = 0; j < NUM_BMS_ICS; j++) {
      bmbReadOkThisScan[j] &= adbms6830_is_ic_responsive(j);
    }
    for (int j = 0; j < NUM_BMS_ICS; j++)
    {
      if (!adbms6830_is_ic_responsive(j)) continue;
      if (cmd_ID == 4)
      {
        for (int k = 0; k < 2; k++)
        {
          value = (rawData[j * 6 + k * 2 + 1] << 8) | rawData[j * 6 + k * 2];
          voltageData[cmd_ID * 3 + j * CELLS_PER_BMB + k] = convertVoltage(value);
        }
      }
      else
      {
        for (int k = 0; k < 3; k++)
        {
          value = (rawData[j * 6 + k * 2 + 1] << 8) | rawData[j * 6 + k * 2];
          voltageData[cmd_ID * 3 + j * CELLS_PER_BMB + k] = convertVoltage(value);
        }
      }
    }
  }

  // ADBMS read cmd 5-7 for temperatures
  if (cmd_ID >= 5 && cmd_ID <= 8)
  {
    responsiveChips = adbms6830_cmd_read(CMD_RDCs[cmd_ID], rawData);
    for (int j = 0; j < NUM_BMS_ICS; j++) {
      bmbReadOkThisScan[j] &= adbms6830_is_ic_responsive(j);
    }
    for (int j = 0; j < NUM_BMS_ICS; j++)
    {
      if (!adbms6830_is_ic_responsive(j)) continue;
      // For Auxiliary Register Group A, there are 2 temp values in 2nd/3rd voltage value
      if (cmd_ID == 5) {
        for (int k = 0; k < 2; k++) {
          value = (rawData[j * 6 + k * 2 + 3] << 8) | rawData[j * 6 + k * 2 + 2];
          tempData[j * TEMPERATURES_PER_BMB + k] = convertTemp(convertVoltage(value), statVreg[j]);
        }
      }
      // For Auxiliary Register Group B, there are 3 temp values
      if (cmd_ID == 6) {
        for (int k = 0; k < 3; k++) {
          value = (rawData[j * 6 + k * 2 + 1] << 8) | rawData[j * 6 + k * 2];
          tempData[j * TEMPERATURES_PER_BMB + k + 2] = convertTemp(convertVoltage(value), statVreg[j]);
        }
      }
      // For Auxiliary Register Group C, there are 3 temp values
      if (cmd_ID == 7)
      {
        for (int k = 0; k < 3; k++) {
          value = (rawData[j * 6 + k * 2 + 1] << 8) | rawData[j * 6 + k * 2];
          tempData[j * TEMPERATURES_PER_BMB + k + 5] = convertTemp(convertVoltage(value), statVreg[j]);
        }
      }
      // For Auxiliary Register Group D, there is 1 temp value in 1st voltage value
      if (cmd_ID == 8)
      {
        value = (rawData[j * 6 + 1] << 8) | rawData[j * 6];
        tempData[j * TEMPERATURES_PER_BMB + 8] = convertTemp(convertVoltage(value), statVreg[j]);
      }
    }
  }

  if (cmd_ID == 8)
  {
    completeScanResponsiveChips = 0;
    for (int i = 0; i < NUM_BMS_ICS; i++) {
      bmbReadOk[i] = bmbReadOkThisScan[i];
      if (bmbReadOk[i]) completeScanResponsiveChips++;
    }

    // Writes voltage values into CAN Packets
    for (int i = 0; i < ARR_LEN(cellVoltages); i++)
    {
      cellVoltages[i].dlc = 8;
      for (int j = 0; j < 4; j++)
      {
         float v = voltageData[i * 4 + j];
         if(v < 0.1f) v = 0;
        CAN_writeFloat(uint16_t, &cellVoltages[i], j * 2, v, 0.0001f);
        checkMinMaxCells(i * 4 + j);
      }
    }

    // Writes temperature values into CAN Packets
    for (int i = 0; i < ARR_LEN(cellTemps); i++)
    {
      cellTemps[i].dlc = 8;
      for (int j = 0; j < 4; j++)
      {
          float t = tempData[i * 4 + j];
          if(t < 0.1f) t = 0;
        if (i == 22 && j > 1) break;
        CAN_writeFloat(uint16_t, &cellTemps[i], j * 2, t, 0.1f);
        checkMinMaxTemps(i * 4 + j);
      }
    }

    doChecks(state);
    updateBalanceCommands();
  }

  cmd_ID++;
  if (cmd_ID == 9) cmd_ID = -1;
}

bool isIsoSpiResponsive()
{
  static float lastResponsiveTime = -999.0f;
  if (completeScanResponsiveChips == NUM_BMS_ICS)
  {
    lastResponsiveTime = lib_timer_elapsed_ms();
  }
  return (lib_timer_elapsed_ms() - lastResponsiveTime) < 5.0f;
}

void setDeadCells() {
  // all good!
}

void setDeadThermistors() {
  // all good!
}

void doChecks(int state)
{
  (void)state;
  checkCellVoltagesWithinBounds = true;
  packVoltage = 0;

  for (int i = 0; i < numCells; i++)
  {
    packVoltage += voltageData[i];
    if(deadCells[i])
    {
      if (voltageData[i] < -0.5f || voltageData[i] > 0.5f)
      {
        checkCellVoltagesWithinBounds = false;
      }
    } else
    {
      if (voltageData[i] > CELL_OVER_VOLTAGE || voltageData[i] < CELL_UNDER_VOLTAGE)
      {
        checkCellVoltagesWithinBounds = false;
      }
    }
  }

  checkPackVoltageWithinBounds = packVoltage < PACK_OVER_VOLTAGE && packVoltage > PACK_UNDER_VOLTAGE;

  checkTempsWithinBounds = currentMinTemp >= UNDER_TEMP && currentMaxTemp <= OVER_TEMP;

  maxTemp = currentMaxTemp; // save last temp range for data
  minTemp = currentMinTemp;
  currentMaxTemp = -999.0f; // begin next temp range
  currentMinTemp = 999.0f;

  maxCellVoltage = currentMaxVoltage;
  minCellVoltage = currentMinVoltage;
  currentMaxVoltage = -999.0f;
  currentMinVoltage = 999.0f;
}

uint32_t getNumResponsiveChips()
{
  return completeScanResponsiveChips;
}

void checkMinMaxTemps(int tempIndex)
{
  if (deadThermistors[tempIndex]) return;
  float temp = tempData[tempIndex];
  if (currentMaxTemp < temp) currentMaxTemp = temp;
  if (currentMinTemp > temp) currentMinTemp = temp;
}

void checkMinMaxCells(int cellIndex)
{
    if (deadCells[cellIndex]) return;
    float voltage = voltageData[cellIndex];
    if (currentMaxVoltage < voltage) currentMaxVoltage = voltage;
    if (currentMinVoltage > voltage && voltage > 1.0f) currentMinVoltage = voltage;
}

bool areCellVoltagesWithinBounds()
{
  return checkCellVoltagesWithinBounds;
}

bool isPackVoltageWithinBounds()
{
  return checkPackVoltageWithinBounds;
}

float getPackVoltageFromCells()
{
  return packVoltage;
}

bool isTempWithinBounds()
{
  return checkTempsWithinBounds;
}

float getMaxTemp()
{
  return maxTemp;
}

float getMinTemp()
{
  return minTemp;
}

float getCellVoltage(uint32_t cellIndex)
{
  if (cellIndex >= NUM_BMS_ICS * CELLS_PER_BMB) return 0.0f;
  return voltageData[cellIndex];
}

float getCellTemperature(uint32_t temperatureIndex)
{
  if (temperatureIndex >= NUM_BMS_ICS * TEMPERATURES_PER_BMB) return -999.0f;
  return tempData[temperatureIndex];
}

bool isCellVoltageReadingOk(uint32_t cellIndex)
{
  if (cellIndex >= NUM_BMS_ICS * CELLS_PER_BMB) return false;
  return voltageData[cellIndex] >= CELL_UNDER_VOLTAGE &&
         voltageData[cellIndex] <= CELL_OVER_VOLTAGE;
}

bool isCellTemperatureReadingOk(uint32_t temperatureIndex)
{
  if (temperatureIndex >= NUM_BMS_ICS * TEMPERATURES_PER_BMB) return false;
  return tempData[temperatureIndex] >= UNDER_TEMP &&
         tempData[temperatureIndex] <= OVER_TEMP;
}

bool isBmbReadingOk(uint32_t bmbIndex)
{
  return bmbIndex < NUM_BMS_ICS && bmbReadOk[bmbIndex];
}

float convertTemp(float V, float Vreg)
{
  float trueR = (V * 10.0f) / (Vreg - V);
  for (int i = 0; i < 35; i++)
  {
    float r1 = lutRes[i];
    float r2 = lutRes[i + 1];

    if (trueR >= r1 && trueR <= r2)
    {
      float t1 = lutTemp[i];
      float t2 = lutTemp[i + 1];

      // Linear Interpolation
      float interpolatedTemp = t1 + (trueR - r1) / (r2 - r1) * (t2 - t1);
      return interpolatedTemp;
    }
  }
  return -999.0f;
}

float convertVoltage(uint16_t v)
{
  int16_t vs = (int16_t) v;
  float voltage = (vs * 0.00015f) + 1.5f;
  return voltage;
}

void updateBmsLimits(float newMinVoltage, float newMaxVoltage, float newMinTemp, float newMaxTemp)
{
  //  CELL_UNDER_VOLTAGE = newMinVoltage;
  //  CELL_OVER_VOLTAGE = newMaxVoltage;
  //  UNDER_TEMP = newMinTemp;
  //  OVER_TEMP = newMaxTemp;
}

void updateBalanceCommands()
{
  bool readyToBalance = carParked && isIsoSpiResponsive() && areCellVoltagesWithinBounds() && isTempWithinBounds();
  totalBalancing = 0;

  if(readyToBalance)
  {
    volatile float minVoltage = 999.0f;

    for(int i = 0; i < numCells; i++)
    {
      if(deadCells[i]) continue;
      if(voltageData[i] < minVoltage)
      {
        minVoltage = voltageData[i];
      }
    }

    bool reasonableMinVoltage = (minVoltage >= CELL_UNDER_VOLTAGE);

    for(int i = 0; i < numCells; i++)
    {
      if(deadCells[i]) continue;

      if(!reasonableMinVoltage)
      {
        balanceCommands[i] = false;
        continue;
      }

      // if(totalBalancing >= 10)
      // {
      //   balanceCommands[i] = false;
      //   continue;
      // }

      if(voltageData[i] > minVoltage + 0.004f)
      {
        balanceCommands[i] = true;
        totalBalancing++;
      } else if(voltageData[i] < minVoltage + 0.002f)
      {
        balanceCommands[i] = false;
      }
    }
  } else
  {
    for(int i = 0; i < numCells; i++)
    {
      balanceCommands[i] = false;
    }
  }
}
