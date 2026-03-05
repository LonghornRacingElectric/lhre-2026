
#include "AppsProcessor.h"


/**
 * Take APPS raw analog voltages and convert them to pedal travel percentages.
 * Make sure the two sensors are both within range and in agreement with each other.
 *
 * Read the rules!
 */

void AppsProcessor::evaluate(VcuParameters *params, AppsProcessorInput *input,
                             AppsProcessorOutput *output, float deltaTime) {

  bool apps1InRange = (input->apps1 >= params->apps1VoltageMin) && (input->apps1 <= params->apps1VoltageMax);
  bool apps2InRange = (input->apps2 >= params->apps2VoltageMin) && (input->apps2 <= params->apps2VoltageMax);

  if (!apps1InRange || !apps2InRange) {
    rangeClock.count(deltaTime);
    if (rangeClock.isFinished()) {
      output->fault = (!apps1InRange) * APPS_1_OUT_OF_BOUNDS + (!apps2InRange) * APPS_2_OUT_OF_BOUNDS;
      app1Filter.reset();
      app2Filter.reset();
      output->apps1 = 0;
      output->apps2 = 0;
      output->apps = 0;
      output->ok = !(APPS_SHUTDOWN_MASK & output->fault);
      return;
    }
  } else {
    rangeClock.reset();
  }

  if (apps1InRange) app1Filter.add(input->apps1, deltaTime);
  if (apps2InRange) app2Filter.add(input->apps2, deltaTime);

  float app1Perc = app1Filter.get() - params->apps1VoltageMin;
  float app2Perc = app2Filter.get() - params->apps2VoltageMin;
  app1Perc = app1Perc / (params->apps1VoltageMax - params->apps1VoltageMin);
  app2Perc = app2Perc / (params->apps2VoltageMax - params->apps2VoltageMin);
  output->apps1 = app1Perc;
  output->apps2 = app2Perc;

  float diff = app1Perc - app2Perc;
  if (diff < 0) { diff = -diff; }

  if (diff > params->appsPlausibilityRange) {
    differenceClock.count(deltaTime);
    if (differenceClock.isFinished()) {
      output->fault = APPS_DISAGREE;
      output->ok = !(APPS_SHUTDOWN_MASK & output->fault);
      output->apps = 0;
      app1Filter.reset();
      app2Filter.reset();
      return;
    }
  } else {
    differenceClock.reset();
  }

  output->fault = APPS_OK;
  output->ok = !(APPS_SHUTDOWN_MASK & output->fault);

  float appsNoDeadzone = (app1Perc + app2Perc) / 2;
  float slope = 1.0f / (1.0f - params->appsDeadZoneTopPct - params->appsDeadZoneBottomPct);

  if (appsNoDeadzone <= params->appsDeadZoneBottomPct) {
    output->apps = 0;
  } else if (appsNoDeadzone >= (1 - params->appsDeadZoneTopPct)) {
    output->apps = 1;
  } else {
    output->apps = slope * (appsNoDeadzone - params->appsDeadZoneBottomPct);
  }
}

void AppsProcessor::reset() {
  app1Filter.reset();
  app2Filter.reset();
  differenceClock.reset();
  rangeClock.reset();
}

void AppsProcessor::setParameters(VcuParameters *params) {
  this->differenceClock = Timer(params->appsImplausibilityTime);
  this->rangeClock = Timer(params->appsImplausibilityTime);
  this->app1Filter = LowPassFilter(params->appsLowPassFilterTimeConstant);
  this->app2Filter = LowPassFilter(params->appsLowPassFilterTimeConstant);
}
