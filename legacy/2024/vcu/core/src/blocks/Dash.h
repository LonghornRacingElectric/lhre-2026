#ifndef VCU_CORE_DASH_H
#define VCU_CORE_DASH_H


#include "VcuParameters.h"
#include "util/filters/Differentiator.h"
#include "util/filters/LowPassFilter.h"


typedef struct DashInput {
  float wheelSpeedFl;
  float wheelSpeedFr;
} DashInput;


typedef struct DashOutput {
  float speedometer;
  // TODO figure out what dash needs
} DashOutput;


class Dash {
private:
  LowPassFilter speedLpf = LowPassFilter(0.0f);
public:
  void setParameters(VcuParameters *params);

  void evaluate(VcuParameters *params, DashInput *input, DashOutput *output, float deltaTime);
};


#endif //VCU_CORE_DASH_H
