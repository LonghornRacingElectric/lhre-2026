#include "state_machine.h"

#include "cells.h"
#include "charging.h"
#include "contactors.h"
#include "hvc_can.h"
#include "main.h"
#include "state_machine_logic.h"
#include "vct_sense.h"

#ifdef HVC_CAN_INTEGRATION_TEST
#define HVC_CAN_TEST_PACK_VOLTAGE 100.0f
#define HVC_CAN_TEST_TRACTIVE_VOLTAGE 90.0f
#endif

static hvc_state_machine_context_t stateMachine;
static hvc_state_machine_outputs_t stateOutputs;

static void applyOutputs(void) {
    setTractiveContactor(stateOutputs.positiveContactorClosed);
    hvc_control_charging(stateOutputs.chargerEnabled);
}

void state_machine_init(void) {
    hvc_state_machine_reset(&stateMachine, &stateOutputs);
    applyOutputs();
}

void update_state_machine(bool anyFaults) {
    const hvc_state_machine_inputs_t inputs = {
        .anyFaults = anyFaults,
        .shutdownClosed = isShutdownClosed(),
#ifdef HVC_CAN_INTEGRATION_TEST
        /*
         * Exercise the production state logic and real contactor output while
         * the maintenance plugs are removed. These values are used only by
         * the precharge decision; CAN telemetry continues to use real sensors.
         */
        .chargerConnected = false,
        .tractiveVoltage = HVC_CAN_TEST_TRACTIVE_VOLTAGE,
        .packVoltage = HVC_CAN_TEST_PACK_VOLTAGE,
#else
        .chargerConnected = hvc_can_is_charger_connected(),
        .tractiveVoltage = getTractiveVoltage(),
        .packVoltage = getPackVoltageFromCells(),
#endif
        .currentTimeMs = HAL_GetTick(),
    };

    hvc_state_machine_step(&stateMachine, &inputs, &stateOutputs);
    applyOutputs();
}

hvc_state_t get_current_state(void) {
    return stateMachine.state;
}

const char *get_state_name(hvc_state_t state) {
    switch (state) {
      case HVC_STATE_NOT_ENERGIZED:
        return "NOT_ENERGIZED";
      case HVC_STATE_PRECHARGING:
        return "PRECHARGING";
      case HVC_STATE_ENERGIZED:
        return "ENERGIZED";
      case HVC_STATE_CHARGING_PRECHARGING:
        return "CHARGING_PRECHARGING";
      case HVC_STATE_CHARGING:
        return "CHARGING";
      default:
        return "UNKNOWN";
    }
}
