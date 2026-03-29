#ifndef VCU_CORE_APPSPROCESSOR_H
#define VCU_CORE_APPSPROCESSOR_H

#include <stdint.h>

#include "VcuParameters.h"
#include "util/filters/Timer.h"
#include "util/filters/LowPassFilter.h"

#define APPS_OK 0x00
#define APPS_1_OUT_OF_BOUNDS 0x01
#define APPS_2_OUT_OF_BOUNDS 0x02
#define APPS_DISAGREE 0x04

#define APPS_SHUTDOWN_MASK (APPS_1_OUT_OF_BOUNDS | APPS_2_OUT_OF_BOUNDS | APPS_DISAGREE)

typedef struct AppsProcessorInput {
    float apps1; // apps1 voltage (V)
    float apps2; // apps2 voltage (V)
} AppsProcessorInput;

typedef struct AppsProcessorOutput {
    float apps; // pedal travel (%)
    float apps1; // (%)
    float apps2; // (%)
    uint32_t fault;
    bool ok;
} AppsProcessorOutput;

class AppsProcessor {
public:
    void reset();
    void evaluate(VcuParameters* params, AppsProcessorInput* input, AppsProcessorOutput* output, float deltaTime);
    void setParameters(VcuParameters* params);
private:
    Timer rangeClock = Timer(0.1f);
    Timer differenceClock = Timer(0.1f);
    LowPassFilter app1Filter = LowPassFilter(0);
    LowPassFilter app2Filter = LowPassFilter(0);
};


#endif //VCU_CORE_APPSPROCESSOR_H
