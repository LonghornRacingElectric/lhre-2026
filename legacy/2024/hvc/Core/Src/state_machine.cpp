//
// Created by rolandwang on 11/12/2023.
//

#include "state_machine.h"
#include "contactors.h"
#include "vsense.h"
#include "cells.h"


void stateMachineInit() {
  setDriveContactor(false);
  setPrechargeContactor(false);
  currentState = STATE_NOT_ENERGIZED;
}

int updateStateMachine(bool shutdownClosed, bool hvOk, bool chargerPresent, float deltaTime) {
  switch (currentState) {
    case STATE_NOT_ENERGIZED:
      if (shutdownClosed && hvOk) {
        if (!chargerPresent) {
          verifyVoltage = 0.0f;
          setPrechargeContactor(true);
          currentState = STATE_PRECHARGING;
        } else {
          verifyVoltage = 0.0f;
          setPrechargeContactor(true);
          currentState = STATE_CHARGING_PRECHARGING;
        }
      }
      break;

    case STATE_PRECHARGING:
      if (!shutdownClosed || !hvOk) {
        setPrechargeContactor(false);
        setDriveContactor(false);
        currentState = STATE_NOT_ENERGIZED;
      }
      if (getTractiveVoltage() > 0.83f * getPackVoltageFromCells()) {
        verifyVoltage += deltaTime;

        if (verifyVoltage >= 5.0f) {
          setDriveContactor(true);
          setPrechargeContactor(false);
          currentState = STATE_ENERGIZED;
        }
      } else {
        verifyVoltage = 0.0f;
      }
      break;

    case STATE_ENERGIZED:
      if (!shutdownClosed || !hvOk) {
        setDriveContactor(false);
        setPrechargeContactor(false);
        currentState = STATE_NOT_ENERGIZED;
      }
      break;

    case STATE_CHARGING_PRECHARGING:
      if (!shutdownClosed || !hvOk || !chargerPresent) {
        setDriveContactor(false);
        setPrechargeContactor(false);
        currentState = STATE_NOT_ENERGIZED;
      }
      if (getTractiveVoltage() > 0.83f * getPackVoltageFromCells()) {
        verifyVoltage += deltaTime;
        if (verifyVoltage >= 5.0f) {
          setDriveContactor(true);
          setPrechargeContactor(false);
          currentState = STATE_CHARGING;
        }
      } else {
        verifyVoltage = 0.0f;
      }

      break;

    case STATE_CHARGING:
      if (!shutdownClosed || !hvOk || !chargerPresent) {
        setDriveContactor(false);
        currentState = STATE_NOT_ENERGIZED;
      }
      break;

    default:
      setDriveContactor(false);
      currentState = STATE_NOT_ENERGIZED;
  }
  return currentState;
}
