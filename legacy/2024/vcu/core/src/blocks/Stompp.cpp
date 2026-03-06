#include "Stompp.h"

/**
 * STOMPP: Software To Omit Multi-Pedal Presses
 * Also known as EV 5.7, but that's a lame name
 *
 * Read the rules!
 *
 * STOMPP must monitor both BSE (Brake System Encoder) AND APPS (accelerator travel %) signals
 *
 * If both APPS > 25% AND brakes are enabled at the same time:
 * - Power to motors must be shut off
 *
 * To get out of this:
 * - APPS signal must fall under 5%
 */

void Stompp::evaluate(VcuParameters *params, StomppInput *input, StomppOutput *output, float deltaTime) {

  //"stomppActive = false" means "output.ok = true"
  //"output.ok = true" means motor functions normally

  //"stomppActive = true" means "output.ok = false"
  //"output.ok = false" means motor is disabled


  //Check for two main requirements. Enable STOMPP IF:
  //  Brakes are active
  //  Accelerator Travel > 25%
  if ((input->bse > params->brakesSoftThreshold) && (input->apps > params->stomppAppsCutoffThreshold)) {
    output->ok = false;
    stomppActive = true;
  }

  //To disable STOMPP, Accelerator Travel must fall under 5%
  if (stomppActive) {
    output->ok = false;
    if (input->apps < params->stomppAppsRecoveryThreshold) {
      output->ok = true;
      stomppActive = false;
    }
  }

  //Check if STOMPP has been disabled
  if (!stomppActive)
    output->ok = true;

  output->fault = (!output->ok) * STOMPP_FAULT;
}

//Testing purposes
void Stompp::reset() {
  stomppActive = false;
}

//Testing purposes
void Stompp::toggleStompp() {
  stomppActive = true;
}

