#include "TractionControl.h"

/**
 * Reduce the driver's torque request when necessary to maintain traction.
 * This prevents the car from spinning out, giving the driver more confidence in acceleration.
 */

void TractionControl::setParameters(VcuParameters *params) {
    lowPassFeedback = LowPassFilter(params->tcsFeedbackLowPassFilterTimeConstant);
}

void TractionControl::evaluate(VcuParameters *params, TractionControlInput *input, TractionControlOutput *output,
                               float deltaTime) {
    if(!params->tcsEnabled || !input->wheelSpeedsOk) {
        output->regulatedTorqueRequest = input->unregulatedTorqueRequest;
        return;
    }

    float averageFrontVelocity = (input->wheelSpeedFl + input->wheelSpeedFr) / 2.0f;
//    float excessVelocityL = input->wheelSpeedBl - averageFrontVelocity;
//    float excessVelocityR = input->wheelSpeedBr - averageFrontVelocity;
//    float excessVelocity = (excessVelocityL > excessVelocityR) ? excessVelocityL : excessVelocityR;
    float averageRearVelocity = (input->motorRpm / 4.0f) * (2.0f * 3.14159f / 60.0f);
    float excessVelocity = averageRearVelocity - averageFrontVelocity;

    float excessSlip = (excessVelocity / averageFrontVelocity) - params->tcsTargetSlipRatio;

    if(excessSlip < 0 || excessVelocity < 0.01f) {
        excessSlip = 0;
    }
    excessVelocity = excessSlip * averageFrontVelocity;
    lowPassFeedback.add(excessVelocity, deltaTime);
    excessVelocity = lowPassFeedback.get();

    float negativeFeedback = params->tcsProportionalGain * excessVelocity;

    output->regulatedTorqueRequest = input->unregulatedTorqueRequest - negativeFeedback;
    if(output->regulatedTorqueRequest < 0.0f) {
      output->regulatedTorqueRequest = 0.0f;
    }
}
