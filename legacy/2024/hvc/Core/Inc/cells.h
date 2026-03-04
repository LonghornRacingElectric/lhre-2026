//
// Created by rolandwang on 11/12/2023.
//

#ifndef HVC_FIRMWARE_2024_CELLS_H
#define HVC_FIRMWARE_2024_CELLS_H


#define PRINT_BMS_DATA

#include <map>
#include <cstdint>
#include "adbms.h"

// Variables
static uint8_t rawData[60];    // Raw data from ADBMS read command
static volatile float voltageData[140]; // 5 segments, 28 cells per segment
static volatile float tempData[50];     // 5 thermistors per ADBMS6830 chip, 10 chips
static volatile float statVreg[NUM_BMS_ICS];
static volatile float statSumCells[NUM_BMS_ICS];
static volatile float statInternalTemps[NUM_BMS_ICS];
static volatile float statAnalogSupply[NUM_BMS_ICS];
static volatile float statDigitalSupply[NUM_BMS_ICS];
static volatile float adowVoltageUp[14 * NUM_BMS_ICS];
static volatile float adowVoltageDown[14 * NUM_BMS_ICS];
static volatile bool openWires[14 * NUM_BMS_ICS];
static bool balanceCommands[14 * NUM_BMS_ICS];
static bool deadCells[140];
static bool deadThermistors[50];

static float currentMinTemp = 999.0f;
static float currentMaxTemp = -999.0f;
static float minAllowedTemp = 0.0f;
static float maxAllowedTemp = 60.0f;
static float minTemp;
static float maxTemp;
static volatile float packVoltage;
static int numCells = 14 * NUM_BMS_ICS;
static int numThermistors = 5 * NUM_BMS_ICS;

static bool checkCellVoltagesWithinBounds = false;
static bool checkPackVoltageWithinBounds = false;
static bool checkTempsWithinBounds = false;

inline bool carParked = false;
static int totalBalancing = 0;

static float minCellVoltage;
static float maxCellVoltage;
static float currentMinVoltage = 999.0f;
static float currentMaxVoltage = -999.0f;

// Functions
void cellsInit();
void cellsPeriodic(int state);
void checkMinMaxTemps(int tempIndex);
void checkMinMaxCells(int cellIndex);
bool isIsoSpiResponsive();
uint32_t getNumResponsiveChips();
bool areCellVoltagesWithinBounds();
bool isPackVoltageWithinBounds();
bool isTempWithinBounds();
float getPackVoltageFromCells();
float getMaxTemp();
float getMinTemp();
float convertTemp(float V, float Vreg);
float convertVoltage(uint16_t voltage);
static void doChecks(int state);
static void setDeadCells();
static void setDeadThermistors();
static void fixFloating();
void updateBmsLimits(float newMinVoltage, float newMaxVoltage, float newMinTemp, float newMaxTemp);
void updateBalanceCommands();
float getMaxCellMargin();

// Resistance (kOhm), Temperature (C)
static float lutRes[36] = {
        0.264f, 0.299f, 0.34f, 0.388f, 0.444f, 0.51f, 0.587f, 0.678f, 0.786f,
        0.916f, 1.07f, 1.256f, 1.48f, 1.751f, 2.082f, 2.487f, 2.985f,
        3.601f, 4.367f, 5.325f, 6.53f, 8.056f, 10.0f, 12.493f,
        15.713f, 19.902f, 25.391f, 32.64f, 42.292f, 55.253f, 72.809f,
        96.807f, 129.925f, 176.082f, 241.072f, 333.56f
};
static float lutTemp[36] = {
        135.0f, 130.0f, 125.0f, 120.0f, 115.0f, 110.0f, 105.0f, 100.0f,
        95.0f, 90.0f, 85.0f, 80.0f, 75.0f, 70.0f, 65.0f, 60.0f, 55.0f, 50.0f,
        45.0f, 40.0f, 35.0f, 30.0f, 25.0f, 20.0f, 15.0f, 10.0f, 5.0f, 0.0f,
        -5.0f, -10.0f, -15.0f, -20.0f, -25.0f, -30.0f, -35.0f, -40.0f
};

#endif //HVC_FIRMWARE_2024_CELLS_H
