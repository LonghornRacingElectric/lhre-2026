#include "WheelMagnets.h"

float thresh = 0.3f;

WheelTracker dispFr{false, 0, 0, 0};
WheelTracker dispFl{false, 0, 0, 0};
WheelTracker dispBr{false, 0, 0, 0};
WheelTracker dispBl{false, 0, 0, 0};

void WheelMagnets::setParameters(VcuParameters *params) {
    speedFilterFl = LowPassFilter(params->wheelMagnetLpfTimeConstant);
    speedFilterFr = LowPassFilter(params->wheelMagnetLpfTimeConstant);
    speedFilterBl = LowPassFilter(params->wheelMagnetLpfTimeConstant);
    speedFilterBr = LowPassFilter(params->wheelMagnetLpfTimeConstant);
}


float calcSpeed(WheelTracker *tracker, float field, Differentiator& differentiator,
                float deltaTime, LowPassFilter& filter, float timeConstant) {
    if (!tracker->isHigh && field > thresh) {
        tracker->isHigh = true;
        tracker->displacement += 3.14159f / 3.0f;
    } else if (tracker->isHigh && field < -1 * thresh) {
        tracker->isHigh = false;
        tracker->displacement += 3.14159f / 3.0f;
    }
    tracker->tickDuration += deltaTime;
    float dydx = differentiator.get(tracker->displacement, tracker->tickDuration);
    if(dydx != 0){
        filter.add(dydx, tracker->tickDuration);
        tracker->lastTickDuration = tracker->tickDuration;
        tracker->tickDuration = 0;
    }
    float angularVelocity = filter.get();

    float nextEstimatedDuration = (tracker->tickDuration > tracker->lastTickDuration) ? tracker->tickDuration : tracker->lastTickDuration;
    dydx = (3.14159f / 3.0f) / nextEstimatedDuration;
    float alpha = nextEstimatedDuration / (timeConstant + nextEstimatedDuration);
    angularVelocity = (angularVelocity * (1 - alpha)) + (dydx * alpha);

    return angularVelocity;
}

void WheelMagnets::evaluate(VcuParameters *params, WheelMagnetsInput *input, WheelMagnetsOutput *output, float deltaTime) {
    output->ok = true;
    output->fault = WHEEL_MAGNETS_OK;
//  output->wheelSpeedFr = 0;
//  output->wheelSpeedFl = 0;
//  output->wheelSpeedBr = 0;
//  output->wheelSpeedBl = 0;


    if (!input->areSensorsOk) {
        output->fault |= WHEEL_MAGNETS_MISSING;
        output->ok = false;
        return;
    }

    output->wheelSpeedFr = calcSpeed(&dispFr, input->wheelMagneticFieldFr, differentiatorFr, deltaTime,
                                     speedFilterFr, params->wheelMagnetLpfTimeConstant);
    output->wheelSpeedFl = calcSpeed(&dispFl, input->wheelMagneticFieldFl, differentiatorFl, deltaTime,
                                     speedFilterFl, params->wheelMagnetLpfTimeConstant);
    output->wheelSpeedBr = calcSpeed(&dispBr, input->wheelMagneticFieldBr, differentiatorBr, deltaTime,
                                     speedFilterBr, params->wheelMagnetLpfTimeConstant);
    output->wheelSpeedBl = calcSpeed(&dispBl, input->wheelMagneticFieldBl, differentiatorBl, deltaTime,
                                     speedFilterBl, params->wheelMagnetLpfTimeConstant);

}

