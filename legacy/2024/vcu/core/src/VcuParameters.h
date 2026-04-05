
#ifndef VCU_CORE_VCUPARAMETERS_H
#define VCU_CORE_VCUPARAMETERS_H

#include "util/parameters/CurveParameter.h"
#include "util/parameters/SurfaceParameter.h"

/**
 * These are the tunable values of the VCU.
 *
 * They can either be numbers (float), curves (CurveParameter) or surfaces/tables (SurfaceParameter).
 *
 * These parameters are ideally getting loaded from the VCU's nonvolatile memory
 * and getting updated by Texas Tune (or are coming from the sim).
 */

typedef struct VcuParameters {

  // ===== APPS PARAMETERS =====
  float appsLowPassFilterTimeConstant = 0.050f; // (s)
  float appsImplausibilityTime = 0.100f; // (s) fixed my rules
  float appsPlausibilityRange = 0.10f; // (%) // prev 0.2
  float apps1VoltageMin = 0.60f; // voltage corresponding to 0% pedal travel (V)
  float apps1VoltageMax = 1.64f; // voltage corresponding to 100% pedal travel (V)
  float apps2VoltageMin = 0.30f; // voltage corresponding to 0% pedal travel (V)
  float apps2VoltageMax = 0.83f; // voltage corresponding to 100% pedal travel (V)
  float appsDeadZoneBottomPct = 0.12f; // (%) of travel that is 0 or 100
  float appsDeadZoneTopPct = 0.13f; // (%) of travel that is 0 or 100

  // ===== BSE PARAMETERS =====
  float bseLowPassFilterTimeConstant = 0.050f; // (s)
  float bseImplausibilityTime = 0.100f; // (s)
  float bseVoltageMin = 0.45f; // voltage corresponding to 0% brake pressure (V)
  float bseVoltageMax = 4.55f; // voltage corresponding to 100% brake pressure (V)

  // ===== BRAKE PARAMETERS =====
  float brakesSoftThreshold = 0.02f; // minimum brakes to count as mechanical brakes engaged for EV 4.7, also brake light (%)
  float brakesHardThreshold = 0.08f; // hard braking (%)

  // ===== STOMPP PARAMETERS =====
  float stomppAppsCutoffThreshold = 0.25f; // apps pedal travel where motor power needs to be cut off (%)
  float stomppAppsRecoveryThreshold = 0.05f; // apps pedal travel where motor power can be restored (%)

  // ===== TORQUE MAP PARAMETERS =====
  CurveParameter mapPedalToTorqueRequest = CurveParameter(1.0f,
                                                          230.0f); // torque request (Nm) as a function of pedal travel (%)
  CurveParameter mapDerateMotorTemp; // allowed torque (%) as a function of motor temperature (deg C)
  CurveParameter mapDerateInverterTemp; // allowed torque (%) as a function of inverter temperature (deg C)
  CurveParameter mapDerateBatteryTemp; // allowed torque (%) as a function of battery temperature (deg C)
  CurveParameter mapDerateBatterySoc; // allowed torque (%) as a function of battery SoC (%)
  float mapPowerLimit = 50000; // power limit (W) (DE-RATED)
  float mapPowerLimit_kP = 0.0f; // PID proportional gain (Nm/W)
  float mapPowerLimit_kI = 0.15f; // PID integral gain (Nm/(Ws))
    float mapPowerLimit_kD = 0.01;
  // ===== PRNDL PARAMETERS =====
  float prndlBuzzerDuration = 0.25f; // how long the buzzer buzzes (s)
  float prndlSwitchDebounceDuration = 0.100f; // how long a digital high/low must be sustained to be considered (s)

  // ==== COOLING PARAMETERS =====
  CurveParameter  coolingRadiatorFanCurve;
  CurveParameter  coolingBatteryFanCurve;


  // ==== TRACTION CONTROL PARAMETERS =====
  bool tcsEnabled = false;
  float tcsFeedbackLowPassFilterTimeConstant = 0.050f;
  float tcsTargetSlipRatio = 0.12f;
  float tcsProportionalGain = 5.0f;

  // ==== STEERING PARAMETERS ====
  CurveParameter steeringWheelToOuterWheel;
  CurveParameter steeringWheelToInnerWheel;
  float steeringPotMinVoltage = 0.0f;
  float steeringPotMaxVoltage = 2.59f;
  float steeringWheelMinAngle = -99.0f;
  float steeringWheelMaxAngle = 122.0f;

  // ==== SOC PARAMETERS ====
  float socCurrentLpfTimeConstant = 0.500f; // (s)
  CurveParameter socHvVoltageCurve = CurveParameter(600.0f, 1.0f); // HV SoC (%) as a function of voltage (V)
  float socHvRemembered = -1.0f; // remembered HV SoC from last time, negative if N/A (%)
  float socHvLowCurrent = 0.500f; // HV current that's low enough to treat bus voltage as open circuit voltage (A)
  float socHvCharge = 40407.84f; // HV battery charge (Coulombs)
  CurveParameter socLvVoltageCurve = CurveParameter(30.0f, 1.0f); // LV SoC (%) as a function of voltage (V)
  float socLvRemembered = -1.0f; // remembered LV SoC from last time, negative if N/A (%)
  float socLvLowCurrent = 0.500f; // LV current that's low enough to treat bus voltage as open circuit voltage (A)
  float socLvCharge = 30000.0f; // LV battery charge (Coulombs)

  // ==== DASH PARAMETERS ====
  float dashSpeedLpfTimeConstant = 0.050f; // (s)
  float dashTireRadius = 0.2f; // (m)

  // ==== WHEEL MAGNET PARAMETERS ====
  float wheelMagnetLpfTimeConstant = 0.050f; // (s)

} VcuParameters;

#endif //VCU_CORE_VCUPARAMETERS_H
