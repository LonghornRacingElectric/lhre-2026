#ifndef VCU_CORE_2024_VCU_CORE_H
#define VCU_CORE_2024_VCU_CORE_H


#include "VcuParameters.h"
#include "util/Structs.h"
#include "blocks/AppsProcessor.h"
#include "blocks/BseProcessor.h"
#include "blocks/Stompp.h"
#include "blocks/TorqueMap.h"
#include "blocks/TractionControl.h"
#include "blocks/SoftShutdown.h"
#include "blocks/TrackPositioning.h"
#include "blocks/DragReductionSystem.h"
#include "blocks/Prndl.h"
#include "blocks/Steering.h"
#include "blocks/Cooling.h"
#include "blocks/Indicators.h"
#include "blocks/SocEstimation.h"
#include "blocks/Dash.h"
#include "blocks/WheelMagnets.h"

// https://www.figma.com/file/z98vFbTBytWElKBb5sTkwk/Lapsim-v2024-Architecture?type=whiteboard&node-id=0%3A1&t=05V7HnOWgZNBNB2B-1


typedef struct VcuInput {
  float apps1; // apps1 voltage (V)
  float apps2; // apps2 voltage (V)

  float bse1; // bse1 voltage (V)
  float bse2; // bse2 voltage (V)

  float steeringWheelPotVoltage; // (V)

  float motorRpm; // (rpm)

  float wheelMagneticFieldFl; // magnetic flux density (mT)
  float wheelMagneticFieldFr;
  float wheelMagneticFieldBl;
  float wheelMagneticFieldBr;

  float motorTemp; // (deg C)
  float inverterTemp;
  float hvBatteryTemp;

  float hvBatterySoc; // (%)
  bool inverterReady; // (true = ready)

  float hvBatteryVoltage; // (V)
  float hvBatteryCurrent; // (A)

  float lvBatteryVoltage; // (V)
  float lvBatteryCurrent; // (A)

  bool driveSwitch; // (true = D, false = P)

  double gpsLat; // degrees
  double gpsLong; // degrees
  float gpsSpeed; // knots
  float gpsHeading; // degrees

  xyz imu1Accel; // accel x, y, z (m/s^2)
  xyz imu2Accel;
  xyz imu3Accel;
  xyz imu1Gyro; // gyro x, y, z (rad/s)
  xyz imu2Gyro;
  xyz imu3Gyro;
} VcuInput;


typedef struct VcuOutput {
  bool enableInverter; // (true = enable)
  float inverterTorqueRequest; // torque (Nm)

  bool prndlState; // (true = drive)
  bool r2dBuzzer; // (true = buzz)
  float brakeLight; // (%)

  bool enableDragReduction; // (true = enable)

  float pumpOutput; // (%)
  float radiatorOutput; // (%)
  float batteryFansOutput; // (%)

  xyz vehicleDisplacement; // x, y, z (m)
  xyz vehicleVelocity; // x, y, z (m/s)
  xyz vehicleAcceleration; // x, y, z (m/s^2)

  float hvBatterySoc; // (%)
  float lvBatterySoc; // (%)

  float dashSpeed; // (mph)

  float telemetryApps1;
  float telemetryApps2;
  float telemetryApps;
  float telemetryBse1;
  float telemetryBse2;
  float telemetryBse;
  float telemetryWheelSpeedFl;
  float telemetryWheelSpeedFr;
  float telemetryWheelSpeedBl;
  float telemetryWheelSpeedBr;
  float telemetrySteeringWheel;

  // new stuff for power limit
  float telemetryOcvEstimate;
  float telemetryPowerLimit;
  float telemetryPowerLimitFeedbackP;
  float telemetryPowerLimitFeedbackI;
  float telemetryPowerLimitFeedbackD;
  float telemetryPowerLimitFeedbackTorque;

  uint16_t flags;
} VcuOutput;


class VcuModel {
private:
  VcuParameters *params;

  AppsProcessor appsProcessor;
  AppsProcessorInput appsProcessorInput;
  AppsProcessorOutput appsProcessorOutput;

  BseProcessor bseProcessor;
  BseProcessorInput bseProcessorInput;
  BseProcessorOutput bseProcessorOutput;

  Stompp stompp;
  StomppInput stomppInput;
  StomppOutput stomppOutput;

  TorqueMap torqueMap;
  TorqueMapInput torqueMapInput;
  TorqueMapOutput torqueMapOutput;

  TractionControl tractionControl;
  TractionControlInput tractionControlInput;
  TractionControlOutput tractionControlOutput;

  Prndl prndl;
  PrndlInput prndlInput;
  PrndlOutput prndlOutput;

  Steering steering;
  SteeringInput steeringInput;
  SteeringOutput steeringOutput;

  TrackPositioning trackPositioning;
  TrackPositioningInput trackPositioningInput;
  TrackPositioningOutput trackPositioningOutput;

  DragReductionSystem drs;
  DragReductionSystemInput drsInput;
  DragReductionSystemOutput drsOutput;

  Cooling cooling;
  CoolingInput coolingInput;
  CoolingOutput coolingOutput;

  Indicators indicators;
  IndicatorsInput indicatorsInput;
  IndicatorsOutput indicatorsOutput;

  SocEstimation socEstimation;
  SocEstimationInput socEstimationInput;
  SocEstimationOutput socEstimationOutput;

  SoftShutdown softShutdown;
  SoftShutdownInput softShutdownInput;
  SoftShutdownOutput softShutdownOutput;

  Dash dash;
  DashInput dashInput;
  DashOutput dashOutput;

  WheelMagnets wheelMagnets;
  WheelMagnetsInput wheelMagnetsInput;
  WheelMagnetsOutput wheelMagnetsOutput;

public:
  void setParameters(VcuParameters *newParams);

  void evaluate(VcuInput *vcuInput, VcuOutput *vcuOutput, float deltaTime);
};


#endif //VCU_CORE_2024_VCU_CORE_H
