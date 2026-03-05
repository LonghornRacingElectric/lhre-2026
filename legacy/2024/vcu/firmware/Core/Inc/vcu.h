#ifndef VCU_FIRMWARE_2024_VCU_H
#define VCU_FIRMWARE_2024_VCU_H

#include "VcuModel.h"
#include "adc.h"
#include "hvc.h"
#include "pdu.h"
#include "inverter.h"
#include "wheelspeeds.h"
#include "all_imus.h"
#include "gps.h"
#include "drive_switch.h"

static VcuInput vcuInput;
static VcuModel vcuModel;

void vcu_init(VcuParameters& params);

/**
 * Executes VCU Core.
 */
void vcu_execute(AnalogVoltages &analogVoltages, DriveSwitchState &driveSwitchState, HvcStatus &hvcStatus,
                 PduStatus &pduStatus, InverterStatus &inverterStatus, WheelMagnetValues &wheelMagnetValues,
                 ImuData &imuData, GpsData &gpsData, VcuOutput &output, float deltaTime);

/**
 * Update parameters. Do not do this during normal operation; only do this while in park!
 */
void vcu_updateParameters(VcuParameters &params);

#endif //VCU_FIRMWARE_2024_VCU_H
