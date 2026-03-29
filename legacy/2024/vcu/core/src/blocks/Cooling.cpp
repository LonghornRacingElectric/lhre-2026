#include "Cooling.h"

void Cooling::setParameters(VcuParameters *params) {

}

void Cooling::evaluate(VcuParameters *params, CoolingInput *input, CoolingOutput *output, float deltaTime) {
  if(input->parkOrDrive) {
    output->pumpOutput = 1.0f;
    output->radiatorOutput = 0.3f;//params->coolingRadiatorFanCurve(input->motorTemp);
    output->batteryFansOutput = 1.0f;//params->coolingBatteryFanCurve(input->batteryTemp);
  } else {
    output->pumpOutput = 0.0f;
    output->radiatorOutput = 0.0f;
    output->batteryFansOutput = 0.0f;
  }
}
