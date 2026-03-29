#ifndef VCU_CORE_COOLING_H
#define VCU_CORE_COOLING_H


#include "VcuParameters.h"


typedef struct CoolingInput {
    float batteryTemp;
    float inverterTemp;
    float motorTemp;
    bool parkOrDrive;
} CoolingInput;


typedef struct CoolingOutput {
    float pumpOutput;
    float radiatorOutput;
    float batteryFansOutput;
} CoolingOutput;


class Cooling {
public:
    void setParameters(VcuParameters* params);
    void evaluate(VcuParameters *params, CoolingInput *input, CoolingOutput *output, float deltaTime);
};


#endif //VCU_CORE_COOLING_H
