#include "SocEstimation.h"
#include "util/filters/LowPassFilter.h"

void SocEstimation::setParameters(VcuParameters *params) {
  if (params->socHvRemembered > 0.0f) {
    this->hvBatterySoc = params->socHvRemembered;
  }
  if (params->socLvRemembered > 0.0f) {
    this->lvBatterySoc = params->socLvRemembered;
  }

  this->hvBatteryCurrentLpf = LowPassFilter(params->socCurrentLpfTimeConstant);
  this->lvBatteryCurrentLpf = LowPassFilter(params->socCurrentLpfTimeConstant);
}

void SocEstimation::estimateSoc(float voltage, float current, float &soc, CurveParameter &curve, LowPassFilter &lpf,
                             float totalCharge, float lowCurrent, float deltaTime) {
  lpf.add(current, deltaTime);
  float currentLpf = lpf.get();
  bool isCurrentLow = (currentLpf < lowCurrent) && (currentLpf > -lowCurrent);
  bool isSocValid = soc > 0.0f;

  if (isCurrentLow || !isSocValid) {
    soc = curve(voltage);
  } else {
    soc -= current * deltaTime / totalCharge;
  }
}

void SocEstimation::evaluate(VcuParameters *params, SocEstimationInput *input, SocEstimationOutput *output,
                             float deltaTime) {

  estimateSoc(input->hvBatteryVoltage, input->hvBatteryCurrent, this->hvBatterySoc, params->socHvVoltageCurve,
           hvBatteryCurrentLpf, params->socHvCharge, params->socHvLowCurrent, deltaTime);
  output->hvBatterySoc = hvBatterySoc;
  params->socHvRemembered = hvBatterySoc;

  estimateSoc(input->lvBatteryVoltage, input->lvBatteryCurrent, this->lvBatterySoc, params->socLvVoltageCurve,
           lvBatteryCurrentLpf, params->socLvCharge, params->socLvLowCurrent, deltaTime);
  output->lvBatterySoc = lvBatterySoc;
  params->socLvRemembered = lvBatterySoc;

}
