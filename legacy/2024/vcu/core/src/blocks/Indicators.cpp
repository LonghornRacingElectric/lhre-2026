#include "Indicators.h"
#include <cstdint>
#include <cmath>

void Indicators::setParameters(VcuParameters *params) {

}

void Indicators::evaluate(VcuParameters *params, IndicatorsInput *input, IndicatorsOutput *output, float deltaTime) {
  float percent = 0;

  float tau = 0.1f;
  float maxPercent = 0.5f;

  float alpha = deltaTime / (deltaTime + tau);
  accumulator = (1.0f - alpha) * accumulator + alpha * input->bse;
  bool hardBraking = accumulator > params->brakesHardThreshold || accumulator < -params->brakesHardThreshold;
  bool braking = accumulator > params->brakesSoftThreshold || accumulator < -params->brakesSoftThreshold;

  if (hardBraking) {
    bool pattern = (brakeTimer > 0.5f) || (static_cast<uint32_t>(brakeTimer / 0.05f) % 2 == 0);
    percent = pattern * maxPercent;
    brakeTimer += deltaTime;
  } else if (braking) {
    percent = maxPercent;
//  } else if (!input->parkOrDrive) {
//    time += deltaTime;
//    float sineWave = 0.5f * (sin(6.28f / 2.0f * (time - 1.0f)) + 1.0f);
//    float pct = sineWave * 0.90f + 0.10f;
//    percent = (pct * pct * 0.4f);
//    brakeTimer = 0;
  } else {
    percent = 0.0005f;
    brakeTimer = 0;
  }

  output->brakeLight = percent;
}
