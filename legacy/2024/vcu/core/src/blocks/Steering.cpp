#include "Steering.h"
#include "util/MathUtils.h"

/**
 * We have a potentiometer on the steering wheel that we're using the measure the steering wheel angle.
 * From the steering wheel angle, we can infer what angle the wheels are at. It's not actually proportional;
 * You should talk to dynamics and ask them how to translate a steering wheel angle to the left and right tire angles.
 *
 * Output the steering wheel angle, the four tire angles, and the boolean ok = true if the potentiometer
 * isn't acting up.
*/

void Steering::evaluate(VcuParameters *params, SteeringInput *input, SteeringOutput *output, float deltaTime) {
  output->steeringWheelAngle = map(input->steeringPotVoltage,
                                   params->steeringPotMinVoltage, params->steeringPotMaxVoltage,
                                   params->steeringWheelMinAngle, params->steeringWheelMaxAngle);

  if (output->steeringWheelAngle > 0) {
    output->wheelAngleFl = params->steeringWheelToInnerWheel(output->steeringWheelAngle);
    output->wheelAngleFr = params->steeringWheelToOuterWheel(output->steeringWheelAngle);
  } else if (output->steeringWheelAngle < 0) {
    output->wheelAngleFl = -params->steeringWheelToOuterWheel(-output->steeringWheelAngle);
    output->wheelAngleFr = -params->steeringWheelToInnerWheel(-output->steeringWheelAngle);
  } else if (output->steeringWheelAngle == 0) {
    output->wheelAngleFl = 0;
    output->wheelAngleFr = 0;
  }
  output->wheelAngleBl = 0; // the rears will very likely be 0, but rear wheel steering is permitted by the rules :0
  output->wheelAngleBr = 0;
}

