#ifndef VCU_CORE_WHEELMAGNETS_H
#define VCU_CORE_WHEELMAGNETS_H

#include <stdint.h>

#include "VcuParameters.h"
#include "util/filters/Differentiator.h"
#include "util/filters/LowPassFilter.h"

#define WHEEL_MAGNETS_OK 0x00
#define WHEEL_MAGNETS_MISSING 0x01

typedef struct WheelMagnetsInput {
  float wheelMagneticFieldFl; // magnetic flux density (mT)
  float wheelMagneticFieldFr;
  float wheelMagneticFieldBl;
  float wheelMagneticFieldBr;
  bool areSensorsOk; // whether we've received data from all four sensors recently
} WheelMagnetsInput;


typedef struct WheelMagnetsOutput {
  float wheelSpeedFl; // rotational velocity (rad/s)
  float wheelSpeedFr;
  float wheelSpeedBl;
  float wheelSpeedBr;
  uint32_t fault;
  bool ok;
} WheelMagnetsOutput;

typedef struct WheelTracker{
    bool isHigh;
    float displacement;
    float curTime;
    float tickDuration;
    float lastTickDuration;
}WheelTracker;


class WheelMagnets {
private:
  Differentiator differentiatorFl = Differentiator();
  Differentiator differentiatorFr = Differentiator();
  Differentiator differentiatorBl = Differentiator();
  Differentiator differentiatorBr = Differentiator();
  LowPassFilter speedFilterFl = LowPassFilter(0.0f);
  LowPassFilter speedFilterFr = LowPassFilter(0.0f);
  LowPassFilter speedFilterBl = LowPassFilter(0.0f);
  LowPassFilter speedFilterBr = LowPassFilter(0.0f);
public:
  void setParameters(VcuParameters *params);

  void evaluate(VcuParameters *params, WheelMagnetsInput *input, WheelMagnetsOutput *output, float deltaTime);
};


#endif //VCU_CORE_WHEELMAGNETS_H
