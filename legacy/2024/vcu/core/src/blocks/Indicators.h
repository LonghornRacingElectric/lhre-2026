#ifndef VCU_CORE_INDICATORS_H
#define VCU_CORE_INDICATORS_H


#include "VcuParameters.h"


typedef struct IndicatorsInput {
    bool parkOrDrive;
    float bse;
} IndicatorsInput;


typedef struct IndicatorsOutput {
    float brakeLight;
} IndicatorsOutput;


class Indicators {
private:
    float brakeTimer = 0;
    float accumulator = 0;
    float time = 0;
public:
    void setParameters(VcuParameters* params);
    void evaluate(VcuParameters *params, IndicatorsInput *input, IndicatorsOutput *output, float deltaTime);
};


#endif //VCU_CORE_INDICATORS_H
