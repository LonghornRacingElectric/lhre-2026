#ifndef VCU_CORE_TORQUEMAP_H
#define VCU_CORE_TORQUEMAP_H


#include "VcuParameters.h"
#include "util/filters/LowPassFilter.h"


typedef struct TorqueMapInput {
    float apps; // pedal travel (%)
    float motorTemp; // (deg C)
    float motorRpm; // (rpm)
    float inverterTemp; // (deg C)
    float batteryTemp; // (deg C)
    float batterySoc; // (%)
    float batteryVoltage; // (V)
    float batteryCurrent; // (A)
} TorqueMapInput;


typedef struct TorqueMapOutput {
    float torqueRequest; // torque (Nm)

    float ocvEstimate; // open circuit voltage (V)
    float powerLimit; // power (W)
    float feedbackP; // torque (Nm)
    float feedbackI; // torque (Nm)
    float feedbackD; // torque (Nm)
    float feedbackTorque; // torque (Nm)
} TorqueMapOutput;


class TorqueMap {
private:
    LowPassFilter openCircuitVoltageFilter = LowPassFilter(1.000f);
    LowPassFilter currentOvershootFilter = LowPassFilter(0.200f);
    float integral = 0.0f;
    float prevError = 0.0f;
public:
    void setParameters(VcuParameters* params);
    void evaluate(VcuParameters *params, TorqueMapInput *input, TorqueMapOutput *output, float deltaTime);
};


#endif //VCU_CORE_TORQUEMAP_H
