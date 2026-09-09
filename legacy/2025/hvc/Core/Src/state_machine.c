//
// Created by rolan on 4/6/2025.
//

#include "state_machine.h"
#include "contactors.h"
#include "vct_sense.h"
#include "cells.h"

static int currentState = 1;
static float verifyVoltage = 0.0f;

void state_machine_init() {
  setTractiveContactor(false);
  currentState = STATE_NOT_ENERGIZED;
}

int update_state_machine(bool shutdownClosed, bool hvOk, bool chargerPresent, float deltaTime) {
  switch (currentState) {
    case STATE_NOT_ENERGIZED:
      if (shutdownClosed && hvOk) {
        verifyVoltage = 0.0f;
        currentState = STATE_PRECHARGING;
      }
      break;

    case STATE_PRECHARGING:
      if (!shutdownClosed || !hvOk) {
        setTractiveContactor(false);
        currentState = STATE_NOT_ENERGIZED;
      }
      static float timer = 0;
      timer += deltaTime;
      if (getTractiveVoltage() > 0.90f * getPackVoltageFromCells() && timer > 4.0f) {
        verifyVoltage += deltaTime;
        if (verifyVoltage >= 2.0f) {
          setTractiveContactor(true);
          if(chargerPresent) {
            currentState = STATE_CHARGING;
          } else {
            currentState = STATE_ENERGIZED;
          }
          timer = 0.0f;
          // add a fault that detects if it triggered after 5 seconds
        }
      } else {
        verifyVoltage = 0.0f;
      }
      break;

    case STATE_ENERGIZED:
      if (!shutdownClosed || !hvOk) {
        setTractiveContactor(false);
        currentState = STATE_NOT_ENERGIZED;
      }
      break;

    case STATE_CHARGING:
      if (!shutdownClosed || !hvOk || !chargerPresent) {
        setTractiveContactor(false);
        currentState = STATE_NOT_ENERGIZED;
      }
      break;

    default:
      setTractiveContactor(false);
      currentState = STATE_NOT_ENERGIZED;
  }
  return currentState;
}