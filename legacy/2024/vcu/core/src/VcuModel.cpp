#include "VcuModel.h"

void VcuModel::setParameters(VcuParameters *newParams) {
  this->params = newParams;
  this->appsProcessor.setParameters(newParams);
  this->bseProcessor.setParameters(newParams);
  this->stompp.setParameters(newParams);
  this->torqueMap.setParameters(newParams);
  this->tractionControl.setParameters(newParams);
  this->prndl.setParameters(newParams);
  this->steering.setParameters(newParams);
  this->trackPositioning.setParameters(newParams);
  this->drs.setParameters(newParams);
  this->cooling.setParameters(newParams);
  this->indicators.setParameters(newParams);
  this->socEstimation.setParameters(newParams);
  this->dash.setParameters(newParams);
  this->wheelMagnets.setParameters(newParams);
  this->softShutdown.setParameters(newParams);
}

void VcuModel::evaluate(VcuInput *vcuInput, VcuOutput *vcuOutput, float deltaTime) {

  appsProcessorInput = {
      vcuInput->apps1,
      vcuInput->apps2,
  };
  appsProcessor.evaluate(params, &appsProcessorInput, &appsProcessorOutput, deltaTime);

  bseProcessorInput = {
      vcuInput->bse1,
      vcuInput->bse2,
  };
  bseProcessor.evaluate(params, &bseProcessorInput, &bseProcessorOutput, deltaTime);

  stomppInput = {
      appsProcessorOutput.apps,
      bseProcessorOutput.bse,
  };
  stompp.evaluate(params, &stomppInput, &stomppOutput, deltaTime);

  wheelMagnetsInput = {
      vcuInput->wheelMagneticFieldFl,
      vcuInput->wheelMagneticFieldFr,
      vcuInput->wheelMagneticFieldBl,
      vcuInput->wheelMagneticFieldBr,
      true, // TODO use CAN timeout
  };
  wheelMagnets.evaluate(params, &wheelMagnetsInput, &wheelMagnetsOutput, deltaTime);

  torqueMapInput = {
      appsProcessorOutput.apps,
      vcuInput->motorTemp,
      vcuInput->motorRpm,
      vcuInput->inverterTemp,
      vcuInput->hvBatteryTemp,
      vcuInput->hvBatterySoc,
      vcuInput->hvBatteryVoltage,
      vcuInput->hvBatteryCurrent,
  };
  torqueMap.evaluate(params, &torqueMapInput, &torqueMapOutput, deltaTime);

  tractionControlInput = {
      torqueMapOutput.torqueRequest,
      wheelMagnetsOutput.wheelSpeedFl,
      wheelMagnetsOutput.wheelSpeedFr,
      wheelMagnetsOutput.wheelSpeedBl,
      wheelMagnetsOutput.wheelSpeedBr,
      vcuInput->motorRpm,
      wheelMagnetsOutput.ok,
  };
  tractionControl.evaluate(params, &tractionControlInput, &tractionControlOutput, deltaTime);

  steeringInput = {
      vcuInput->steeringWheelPotVoltage,
  };
  steering.evaluate(params, &steeringInput, &steeringOutput, deltaTime);

  prndlInput = {
      vcuInput->driveSwitch,
      vcuInput->inverterReady,
      bseProcessorOutput.bse,
      appsProcessorOutput.apps,
  };
  prndl.evaluate(params, &prndlInput, &prndlOutput, deltaTime);

  trackPositioningInput = {
      vcuInput->gpsLat,
      vcuInput->gpsLong,
      vcuInput->gpsSpeed,
      vcuInput->gpsHeading,

      vcuInput->imu1Accel,
      vcuInput->imu2Accel,
      vcuInput->imu3Accel,

      vcuInput->imu1Gyro,
      vcuInput->imu2Gyro,
      vcuInput->imu3Gyro,

      // TODO remove wheel and steering data once merged, it's not used in EKF
      0,
      0,
      0,
      0,

      steeringOutput.wheelAngleFl,
      steeringOutput.wheelAngleFr,
      steeringOutput.wheelAngleBl,
      steeringOutput.wheelAngleBr,
  };
  trackPositioning.evaluate(params, &trackPositioningInput, &trackPositioningOutput, deltaTime);

  drsInput = {
      steeringOutput.steeringWheelAngle,
      trackPositioningOutput.vehicleVelocity,
  };
  drs.evaluate(params, &drsInput, &drsOutput, deltaTime);

  coolingInput = {
      vcuInput->hvBatteryTemp,
      vcuInput->inverterTemp,
      vcuInput->motorTemp,
      prndlOutput.state
  };
  cooling.evaluate(params, &coolingInput, &coolingOutput, deltaTime);

  indicatorsInput = {
      prndlOutput.state,
      bseProcessorOutput.bse,
  };
  indicators.evaluate(params, &indicatorsInput, &indicatorsOutput, deltaTime);

  socEstimationInput = {
      vcuInput->hvBatteryVoltage,
      vcuInput->hvBatteryCurrent,
      vcuInput->lvBatteryVoltage,
      vcuInput->lvBatteryCurrent,
  };
  socEstimation.evaluate(params, &socEstimationInput, &socEstimationOutput, deltaTime);

  dashInput = {
      wheelMagnetsOutput.wheelSpeedFl,
      wheelMagnetsOutput.wheelSpeedFr,
  };
  dash.evaluate(params, &dashInput, &dashOutput, deltaTime);

  softShutdownInput = {
      appsProcessorOutput.ok,
      bseProcessorOutput.ok,
      stomppOutput.ok,
      prndlOutput.state,
      tractionControlOutput.regulatedTorqueRequest,
  };
  softShutdown.evaluate(params, &softShutdownInput, &softShutdownOutput, deltaTime);

  uint16_t flags = (appsProcessorOutput.fault)
                   | (bseProcessorOutput.fault << 3)
                   | (stomppOutput.fault << 6)
                   | (wheelMagnetsOutput.fault << 7)
                   | (softShutdownOutput.enableInverter << 8)
                   | (prndlOutput.state << 9)
                   | (prndlOutput.buzzer << 10)
                   | ((indicatorsOutput.brakeLight > 0.1) << 11)
                   | (drsOutput.enable << 12);

  *vcuOutput = {
      softShutdownOutput.enableInverter,
      softShutdownOutput.inverterTorqueRequest,

      prndlOutput.state,
      prndlOutput.buzzer,
      indicatorsOutput.brakeLight,

      drsOutput.enable,

      coolingOutput.pumpOutput,
      coolingOutput.radiatorOutput,
      coolingOutput.batteryFansOutput,

      trackPositioningOutput.vehicleDisplacement,
      trackPositioningOutput.vehicleVelocity,
      trackPositioningOutput.vehicleAcceleration,

      socEstimationOutput.hvBatterySoc,
      socEstimationOutput.lvBatterySoc,

      dashOutput.speedometer,

      // additional telemetry
      appsProcessorOutput.apps1,
      appsProcessorOutput.apps2,
      appsProcessorOutput.apps,
      bseProcessorOutput.bse1,
      bseProcessorOutput.bse2,
      bseProcessorOutput.bse,
      wheelMagnetsOutput.wheelSpeedFl,
      wheelMagnetsOutput.wheelSpeedFr,
      wheelMagnetsOutput.wheelSpeedBl,
      wheelMagnetsOutput.wheelSpeedBr,
      steeringOutput.steeringWheelAngle,

      // new power limit telemetry
      torqueMapOutput.ocvEstimate,
      torqueMapOutput.powerLimit,
      torqueMapOutput.feedbackP,
      torqueMapOutput.feedbackI,
      torqueMapOutput.feedbackD,
      torqueMapOutput.feedbackTorque,

      // faults
      flags,
  };

}
