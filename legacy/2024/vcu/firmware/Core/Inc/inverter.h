#ifndef VCU_FIRMWARE_2024_INVERTER_H
#define VCU_FIRMWARE_2024_INVERTER_H

#include "stm32h7xx_hal.h"
#include "VcuModel.h"
#include <string>

typedef struct InverterStatus {
  bool isRecent;
  float voltage; //input voltage into DC side
  float current; //input current into DC side
  float rpm;
  float inverterTemp;
  float motorTemp;

  float motorAngle;
  float resolverAngle;

  float phaseACurrent;
  float phaseBCurrent;
  float phaseCCurrent;

  float BCVoltage;
  float ABVoltage;
  float outputVoltage;

  float inverterFrequency;

  float torqueActual;
  float torqueCommand;

  uint64_t faultVector;
  uint64_t stateVector;

  uint32_t newData; // Comes from params in inverter.
} InverterStatus;

/**
 * Initialize inverter CAN communication.
 */
void inverter_init();

/**
 * Get latest inverter info.
 */
static void inverter_getStatus(InverterStatus *status);

/**
 * Send a torque command over CAN.
 */
static void inverter_updateTorqueCommand(float torque, float rpm, bool enable_inverter);

/**
 * Reset faults over CAN.
 */
void inverter_resetFaults();

/**
 * Read/write inverter parameters over CAN.
 * @param param_addr
 * @param param_value
 * @param write
 */
void inverter_writeParameter(uint16_t param_addr, uint16_t param_value);

/**
 *
 */
void inverter_readParameter(uint16_t address);

/**
 * Read/write inverter parameters over CAN.
 * @param param_addr
 * @param param_value
 * @param write
 */
void inverter_updateParameterCommand(float deltaTime);

/**
 * Periodic method to get status and update torque command.
 */
void inverter_periodic(InverterStatus *status, VcuOutput* vcuCoreOutput, float deltaTime);

#endif //VCU_FIRMWARE_2024_INVERTER_H
