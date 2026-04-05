#include "Dash.h"

void Dash::setParameters(VcuParameters *params) {
  speedLpf = LowPassFilter(params->dashSpeedLpfTimeConstant);
}

void Dash::evaluate(VcuParameters *params, DashInput *input, DashOutput *output, float deltaTime) {
  float wheelSpeedAvg = (input->wheelSpeedFl + input->wheelSpeedFr) / 2.0f;
  speedLpf.add(wheelSpeedAvg, deltaTime);

  float vehicleSpeed = speedLpf.get() * params->dashTireRadius; // (m/s)
  output->speedometer = vehicleSpeed * 2.23694f; // (mph)
}
