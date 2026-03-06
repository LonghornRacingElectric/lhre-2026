#include "BseProcessor.h"
#include "util/MathUtils.h"

/**
 * Take BSE raw analog voltages and convert them to brake pressures.
 * Make sure the two sensors are both within range and in agreement with each other.
 *
 * Technically we only need one functioning BSE for rules compliance so you
 * can discard one sensor's value if it's out of range and just use the other one.
 *
 * Read the rules!
 */



void BseProcessor::evaluate(VcuParameters *params, BseProcessorInput *input,
                            BseProcessorOutput *output, float deltaTime) {

  /*
   * Rules only requires one BSE, so this code will still allow the car to run in the scenario
   * that only one BSE of our two BSEs is working.
   */

  bool bse1Plausible = (input->bse1 >= params->bseVoltageMin) && (input->bse1 <= params->bseVoltageMax);
  bool bse2Plausible = (input->bse2 >= params->bseVoltageMin) && (input->bse2 <= params->bseVoltageMax);

  float bse = 0;
  float div = 0;

  output->fault = BSE_OK;
  output->bse1 = 0;
  output->bse2 = 0;

  if (bse1Plausible) {
    bse1Filter.add(input->bse1, deltaTime);
    float bse1Pressure = map(bse1Filter.get(), params->bseVoltageMin, params->bseVoltageMax,
                             0.0f, 1.0f);
    bse += bse1Pressure;
    div += 1.0f;
    output->bse1 = bse1Pressure;
  } else {
    bse1Filter.reset();
    output->fault |= BSE_1_OUT_OF_BOUNDS;
  }

  if (bse2Plausible) {
    bse2Filter.add(input->bse2, deltaTime);
    float bse2Pressure = map(bse2Filter.get(), params->bseVoltageMin, params->bseVoltageMax,
                             0.0f, 1.0f);
    bse += bse2Pressure;
    div += 1.0f;
    output->bse2 = bse2Pressure;
  } else {
    bse2Filter.reset();
    output->fault |= BSE_2_OUT_OF_BOUNDS;
  }

  if (div == 0) {
    // use last value if within timer, else fault
    clock.count(deltaTime);
    if (clock.isFinished()) {
      output->fault |= BSE_BOTH_OUT_OF_BOUNDS;
    }
    output->ok = !(BSE_SHUTDOWN_MASK & output->fault);
    return;
  } else {
    clock.reset();
  }

  output->bse = bse / div;
  output->ok = !(BSE_SHUTDOWN_MASK & output->fault);
}

void BseProcessor::setParameters(VcuParameters *params) {
  clock = Timer(params->bseImplausibilityTime);
  bse1Filter = LowPassFilter(params->bseLowPassFilterTimeConstant);
  bse2Filter = LowPassFilter(params->bseLowPassFilterTimeConstant);
}

void BseProcessor::reset() {
  clock.reset();
  bse1Filter.reset();
  bse2Filter.reset();
}
