#include "TorqueMap.h"

/**
 * Map a pedal travel percentage to a torque request.
 */

void TorqueMap::evaluate(VcuParameters *params, TorqueMapInput *input, TorqueMapOutput *output, float deltaTime) {
    float torqueRequest = params->mapPedalToTorqueRequest(input->apps);
    float derate;

//    derate = params->mapDerateMotorTemp(input->motorTemp);
//    torqueRequest *= derate;
//
//    derate = params->mapDerateInverterTemp(input->inverterTemp);
//    torqueRequest *= derate;
//
//    derate = params->mapDerateBatteryTemp(input->batteryTemp);
//    torqueRequest *= derate;
//
//    derate = params->mapDerateBatterySoc(input->batterySoc);
//    torqueRequest *= derate;
//

    currentOvershootFilter.add(input->batteryCurrent, deltaTime);
    if(input->batteryCurrent > -1.0f && input->batteryCurrent < 1.0f) {
        openCircuitVoltageFilter.add(input->batteryVoltage, deltaTime);
    }
    float openCircuitVoltage = openCircuitVoltageFilter.get();
    float internalResistance = 0.750; // Ohms, estimated

    float currentLimit = 200.0f; // Amps (reduced from 230A)
    float currentBasedPowerLimit = (openCircuitVoltage - (currentLimit * internalResistance)) * currentLimit;
    float powerLimit = params->mapPowerLimit;
    if(currentBasedPowerLimit < powerLimit) {
        powerLimit = currentBasedPowerLimit;
    }
    if(powerLimit < 0) {
        powerLimit = 0;
    }



    // battery OCV based derate
    derate = std::max(std::min((openCircuitVoltage/126.0f - 3.5f) / 0.1f, 1.0f), 0.0f);
    torqueRequest *= derate;

//    float motorAngularVelocity = input->motorRpm / 60.0f * 2.0f * 3.14159f; // rad/s
//    float maxTorqueAtPowerLimit = powerLimit / motorAngularVelocity * 0.90f; // Nm
//    if(torqueRequest > maxTorqueAtPowerLimit) {
//        torqueRequest = maxTorqueAtPowerLimit;
//    }

    float smoothedCurrent = currentOvershootFilter.get();
    float currentPower = input->batteryVoltage * input->batteryCurrent;
    if(smoothedCurrent > 240.0f || currentPower > 85000.0f) {
        torqueRequest = 0;
    }

    float powerError = powerLimit - currentPower;
    this->integral += powerError * deltaTime;
    if(integral > 0) {
        integral = 0;
    }
    float integralMin = -100.0f;
    if (integral < integralMin) {
        integral = integralMin;
    }
    if(powerError > 0) {
        powerError = 0;
    }
    float derivative = -(powerError - this->prevError)/deltaTime;
    derivative = 0;
    float feedback = params->mapPowerLimit_kP * powerError + params->mapPowerLimit_kI * this->integral + params->mapPowerLimit_kD * derivative;

    if(feedback > 0) {
        feedback = 0;
    }
    torqueRequest += feedback; // feedback is negative
    if(torqueRequest < 0) {
        torqueRequest = 0;
    }

    output->torqueRequest = torqueRequest;

    output->ocvEstimate = openCircuitVoltage;
    output->powerLimit = powerLimit;
    output->feedbackP = powerError;
    output->feedbackI = this->integral;
    output->feedbackD = derivative;
    output->feedbackTorque = feedback;

    this->prevError = powerError;
}

void TorqueMap::setParameters(VcuParameters *params) {

}
