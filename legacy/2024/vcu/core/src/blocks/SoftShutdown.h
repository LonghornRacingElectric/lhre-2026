#ifndef VCU_CORE_SOFTSHUTDOWN_H
#define VCU_CORE_SOFTSHUTDOWN_H


#include "VcuParameters.h"


typedef struct SoftShutdownInput {
    bool appsOk;
    bool bseOk;
    bool stomppOk;
    bool prndlState; // (true = drive)

    float torqueRequest; // torque (Nm)
} SoftShutdownInput;


typedef struct SoftShutdownOutput {
    bool enableInverter;
    float inverterTorqueRequest; // torque (Nm)
} SoftShutdownOutput;


class SoftShutdown {
public:
    void setParameters(VcuParameters* params) {};
    void evaluate(VcuParameters *params, SoftShutdownInput *input, SoftShutdownOutput *output, float deltaTime);
};


#endif //VCU_CORE_SOFTSHUTDOWN_H
