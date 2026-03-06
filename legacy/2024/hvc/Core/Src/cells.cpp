#include "cells.h"
#include "LonghornLib/angel_can.h"
#include "adbms.h"
#include "charging.h"
#include "LonghornLib/clock.h"
#include "usb.h"
#include "state_machine.h"

float PACK_OVER_VOLTAGE = 546.0f;
float PACK_UNDER_VOLTAGE = 390.0f;
float CELL_OVER_VOLTAGE = 4.23f;
float CELL_UNDER_VOLTAGE = 3.00f;
float OVER_TEMP = 60.0f;
float UNDER_TEMP = 0.0f;

// 0-4 is read cell voltages, 5-7 is read cell temperatures
// Commands now for ADBMS6830 (not LTC6813, looks to have similar cmd names)
// Reading 14 cells voltages & 5 temps per chip
static ADBMS6830_Command_t CMD_RDCs[9] = {
  // CMD_RDCVA, CMD_RDCVB, CMD_RDCVC, CMD_RDCVD, CMD_RDCVE,
  CMD_RDFCA, CMD_RDFCB, CMD_RDFCC, CMD_RDFCD, CMD_RDFCE,
  CMD_RDAUXB, CMD_RDAUXC, CMD_RDAUXD
};
static CanOutbox cellVoltages[35];
static CanOutbox cellTemps[13];
static uint32_t responsiveChips = 0;


/**
 * Reads voltage and temperature data from BMS
 * Converts and writes data into respective CanOutboxes
 **/
void cellsPeriodic(int state)
{
  static int divider = 0;
  divider++;
  if(divider == 10)
  {
    divider = 0;
  } else
  {
    return;
  }

  static int cmd_ID = -1; // Used to track and send only 1 ADBMS cmd per iteration
  static uint16_t value = 0;

  adbms6830_wakeup();

  if (cmd_ID == -1)
  {
    adbms6830_wrcfga();
    adbms6830_wrcfgb(false, balanceCommands);

    //adbms6830_adstat();
    // responsiveChips = adbms6830_cmd_read(CMD_RDSTATA, rawData);
    // for (int j = 0; j < responsiveChips; j++)
    // {
    //   statSumCells[j] = static_cast<float>((rawData[j * 6 + 1] << 8) | rawData[j * 6]) * 0.003f;
    //   statInternalTemps[j] =
    //     static_cast<float>((rawData[j * 6 + 3] << 8) | rawData[j * 6 + 2]) * (0.1f / 7.6f) - 276.0f;
    //   statAnalogSupply[j] = static_cast<float>((rawData[j * 6 + 5] << 8) | rawData[j * 6 + 4]) * 0.0001f;
    // }
  }

  //  ltc6813_adowUp();
  //  adbms6830_adcv(0);
  //  for(cmd_ID = 0; cmd_ID < 5; cmd_ID++) {
  //    adbms6830_cmd_read(CMD_RDCs[cmd_ID], rawData);
  //    for (int j = 0; j < NUM_BMS_ICS; j++) {
  //      if (cmd_ID == 4) {
  //        for (int k = 0; k < 2; k++) {
  //          value = (rawData[j * 6 + k * 2 + 1] << 8) | rawData[j * 6 + k * 2];
  //          adowVoltageUp[cmd_ID * 3 + j * 14 + k] = convertVoltage(value);
  //        }
  //      } else {
  //        for (int k = 0; k < 3; k++) {
  //          value = (rawData[j * 6 + k * 2 + 1] << 8) | rawData[j * 6 + k * 2];
  //          adowVoltageUp[cmd_ID * 3 + j * 14 + k] = convertVoltage(value);
  //        }
  //      }
  //    }
  //  }
  //  ltc6813_adowDown();
  //  adbms6830_adcv(0);
  //  for(cmd_ID = 0; cmd_ID < 5; cmd_ID++) {
  //    adbms6830_cmd_read(CMD_RDCs[cmd_ID], rawData);
  //    for (int j = 0; j < NUM_BMS_ICS; j++) {
  //      if (cmd_ID == 4) {
  //        for (int k = 0; k < 2; k++) {
  //          value = (rawData[j * 6 + k * 2 + 1] << 8) | rawData[j * 6 + k * 2];
  //          adowVoltageDown[cmd_ID * 3 + j * 14 + k] = convertVoltage(value);
  //        }
  //      } else {
  //        for (int k = 0; k < 3; k++) {
  //          value = (rawData[j * 6 + k * 2 + 1] << 8) | rawData[j * 6 + k * 2];
  //          adowVoltageDown[cmd_ID * 3 + j * 14 + k] = convertVoltage(value);
  //        }
  //      }
  //    }
  //  }
  //
  //  for (int j = 0; j < NUM_BMS_ICS * 14; j++) {
  //    openWires[j] = (adowVoltageUp[j] - adowVoltageDown[j]) < -0.4f;
  //  }
  //
  //  cmd_ID = 0;

  if (cmd_ID == 0)
  {
    adbms6830_adcv();
    HAL_Delay(10);
    adbms6830_wakeup();
    adbms6830_wrcfgb(true, balanceCommands);
  }
  else if (cmd_ID == 5)
  {
    adbms6830_adax();

    responsiveChips = adbms6830_cmd_read(CMD_RDSTATB, rawData);
    for (int j = 0; j < responsiveChips; j++)
    {
      volatile float vd = convertVoltage((rawData[j * 6 + 1] << 8) | rawData[j * 6]);
      volatile float va = convertVoltage((rawData[j * 6 + 3] << 8) | rawData[j * 6 + 2]);
      volatile float vres = convertVoltage((rawData[j * 6 + 5] << 8) | rawData[j * 6 + 4]);
      statVreg[j] = va;
    }
  }

  // LTC read cmd 0-4 for voltages
  if (cmd_ID >= 0 && cmd_ID <= 4)
  {
    responsiveChips = adbms6830_cmd_read(CMD_RDCs[cmd_ID], rawData);
    for (int j = 0; j < responsiveChips; j++)
    {
      if (cmd_ID == 4)
      {
        for (int k = 0; k < 2; k++)
        {
          value = (rawData[j * 6 + k * 2 + 1] << 8) | rawData[j * 6 + k * 2];
          voltageData[cmd_ID * 3 + j * 14 + k] = convertVoltage(value);
        }
      }
      else
      {
        for (int k = 0; k < 3; k++)
        {
          value = (rawData[j * 6 + k * 2 + 1] << 8) | rawData[j * 6 + k * 2];
          voltageData[cmd_ID * 3 + j * 14 + k] = convertVoltage(value);
        }
      }
    }
  }

  // LTC read cmd 5-8 for temperatures
  if (cmd_ID >= 5 && cmd_ID <= 7)
  {
    responsiveChips = adbms6830_cmd_read(CMD_RDCs[cmd_ID], rawData);

    for (int j = 0; j < responsiveChips; j++)
    {
      // For Auxiliary Register Group B, there is 1 temp value in 3rd voltage value
      if (cmd_ID == 5)
      {
        value = (rawData[j * 6 + 5] << 8) | rawData[j * 6 + 4];
        tempData[j * 5] = convertTemp(convertVoltage(value), statVreg[j]);
      }
      // For Auxiliary Register Group C, there are 3 temp values
      if (cmd_ID == 6)
      {
        for (int k = 0; k < 3; k++)
        {
          value = (rawData[j * 6 + k * 2 + 1] << 8) | rawData[j * 6 + k * 2];
          tempData[j * 5 + k + 1] = convertTemp(convertVoltage(value), statVreg[j]);
        }
      }
      // For Auxiliary Register Group D, there is 1 temp value in 1st voltage value
      if (cmd_ID == 7)
      {
        value = (rawData[j * 6 + 1] << 8) | rawData[j * 6];
        tempData[j * 5 + 4] = convertTemp(convertVoltage(value), statVreg[j]);
      }
    }
  }

  if (cmd_ID == 7)
  {

    fixFloating();

    // Writes voltage values into CanOutboxes
    for (int i = 0; i < 35; i++)
    {
      cellVoltages[i].dlc = 8;
      for (int j = 0; j < 4; j++)
      {
         float v = voltageData[i * 4 + j];
         if(v < 0.1f) v = 0;
        can_writeFloat(uint16_t, &cellVoltages[i], j * 2, v, 0.0001f);
        checkMinMaxCells(i * 4 + j);
      }
    }

    // Writes temperature values into CanOutboxes
    for (int i = 0; i < 13; i++)
    {
      cellTemps[i].dlc = 8;
      for (int j = 0; j < 4; j++)
      {
          float t = tempData[i * 4 + j];
          if(t < 0.1f) t = 0;
        if (i == 12 && j > 1) break;
        can_writeFloat(uint16_t, &cellTemps[i], j * 2, t, 0.1f);
        checkMinMaxTemps(i * 4 + j);
      }
    }

    doChecks(state);
    updateBalanceCommands();
  }

  cmd_ID++;
  if (cmd_ID == 8) cmd_ID = -1;
}

/**
 * Initializes CAN outboxes to send voltage and temp data
 **/
void cellsInit()
{
  setDeadCells();
  setDeadThermistors();

  // Period set to update all values at a frequency of 1 Hz
  can_addOutboxes(HVC_VCU_CELL_VOLTAGES_START, HVC_VCU_CELL_VOLTAGES_END, 1.0f, cellVoltages);
  can_addOutboxes(HVC_VCU_CELL_TEMPS_START, HVC_VCU_CELL_TEMPS_END, 1.0f, cellTemps);
}

bool isIsoSpiResponsive()
{
  static float lastResponsiveTime = -999.0f;
  if (responsiveChips == NUM_BMS_ICS)
  {
    lastResponsiveTime = clock_getTime();
  }
  return (clock_getTime() - lastResponsiveTime) < 5.0f;
}

void setDeadCells()
{
  // segment 1
  deadCells[3] = true;
  deadCells[4] = true;
  deadCells[5] = true;
  deadCells[6] = true;
  deadCells[17] = true;

  // segment 2
  deadCells[1 + 28*1] = true;

  // segment 3
  deadCells[2 + 28*2] = true;
  deadCells[12 + 28*2] = true;

  // segment 4
  deadCells[9 + 28*3] = true;
  deadCells[14 + 28*3] = true;
  deadCells[17 + 28*3] = true;

  // segment 5 (unique)
  deadCells[2 + 28*4] = true; // actually upper half but isospi order flipped
}

void setDeadThermistors()
{
  deadThermistors[10] = true;
}

void fixFloating()
{
  voltageData[7] = voltageData[6] + voltageData[7];
  voltageData[6] = 0;

  // this pin on segment 4 was loose from JLCPCB lol
  voltageData[10 + 28*3] = voltageData[9 + 28*3] + voltageData[10 + 28*3];
  voltageData[9 + 28*3] = 0;
}


void doChecks(int state)
{
  static int count = 0;
  count++;
  bool timeToPrint = ((count % 30) == 0) && (state != STATE_ENERGIZED);

#ifdef PRINT_BMS_DATA
  std::string space = "";
  if (timeToPrint)
  {
    println(space);
  }
#endif

  checkCellVoltagesWithinBounds = true;
  packVoltage = 0;
  for (int i = 0; i < numCells; i++)
  {
    packVoltage += voltageData[i];
#ifdef PRINT_BMS_DATA
    if (timeToPrint)
    {
      if(i % 14 == 0)
      {
        println(space);
      }
      int c = i;
      if(c >= 112)
      {
        if(c <= 125)
        {
          c += 14;
        } else
        {
          c -= 14;
        }
      }
      char b = balanceCommands[c] ? '*' : ' ';
      std::string s = "C" + std::to_string(i+1) + ": " + std::to_string(voltageData[c]) + " V" + b;
      println(s);
      // println(voltageData[c]);
    }
#endif
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

#ifdef PRINT_BMS_DATA
  if (timeToPrint)
  {
    println(space);

    // for (int i = 0; i < numThermistors; i++)
    // {
    //   std::string s = std::to_string(tempData[i]) + " C";
    //   println(s);
    // }
    std::string sMin = "Tmin: " + std::to_string(minTemp) + " C";
    std::string sMax = "Tmax: " + std::to_string(maxTemp) + " C";
    std::string vMin = "CellVmin: " + std::to_string(minCellVoltage) + " V";
    std::string vMax = "CellVmax: " + std::to_string(maxCellVoltage) + " V";
    println(sMin);
    println(sMax);
    println(space);
    println(vMin);
    println(vMax);
    println(space);

    std::string s = std::to_string(totalBalancing) + " cells being discharged";
    println(s);

    println(space);
  }
#endif

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
  return responsiveChips;
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
    if (currentMinVoltage > voltage && voltage > 0.0f) currentMinVoltage = voltage;
}

float getMaxCellMargin()
{
  return CELL_OVER_VOLTAGE - maxCellVoltage;
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
  bool readyToBalance = carParked && isIsoSpiResponsive() && areCellVoltagesWithinBounds() && isTempWithinBounds()
                      && !isChargerPluggedIn();
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

      // if(voltageData[i] > minVoltage + 0.004f)
      if(voltageData[i] > minVoltage + 0.030f)
      {
        balanceCommands[i] = true;
        totalBalancing++;
      // } else if(voltageData[i] < minVoltage + 0.002f)
      } else if(voltageData[i] < minVoltage + 0.020f)
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
