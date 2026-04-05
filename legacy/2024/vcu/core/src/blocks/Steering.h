#ifndef VCU_CORE_STEERING_H
#define VCU_CORE_STEERING_H


#include "VcuParameters.h"


typedef struct SteeringInput {
    float steeringPotVoltage; // (Volts)
} SteeringInput;


typedef struct SteeringOutput {
    float steeringWheelAngle; // (degrees)
    float wheelAngleFl; // (radians)
    float wheelAngleFr;
    float wheelAngleBl;
    float wheelAngleBr;
} SteeringOutput;


class Steering {
public:
    void setParameters(VcuParameters* params) {};
    void evaluate(VcuParameters *params, SteeringInput *input, SteeringOutput *output, float deltaTime);
};


#endif //VCU_CORE_STEERING_H
