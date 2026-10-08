#include "charging.h"

#include "cells.h"
#include "charge_policy.h"
#include "faults.h"
#include "hvc_can.h"
#include "imd.h"

/* Pack voltage the charger regulates to in CV (CHARGE_PACK_TARGET_V). The
   HVC tapers current on the highest cell (charge_policy.c). */
#define PACK_TARGET_VOLTAGE CHARGE_PACK_TARGET_V

static charge_policy_t chargePolicy;
static hvc_charging_status_t chargingStatus;

void hvc_control_charging(bool enable) {
    float chargerVoltage = 0.0f;
    float chargerCurrent = 0.0f;
    bool chargerEnabled = false;
    hvc_can_get_charger_status(&chargerVoltage, &chargerCurrent,
                               &chargerEnabled);

    const charge_inputs_t inputs = {
        .chargingState = enable,
        .chargerConnected = hvc_can_is_charger_connected(),
        .cellDataValid = getNumResponsiveChips() == NUM_BMS_ICS,
        .maxCellV = getMaxCellVoltage(),
        .maxTempC = getMaxTemp(),
        .bmsFault = get_latched_faults() != 0U,
        .imdOk = isImdOk(),
        .chargerCurrentA = chargerCurrent,
    };
    const charge_outputs_t outputs = charge_policy_update(&chargePolicy, &inputs);

    hvc_can_set_charger_command(PACK_TARGET_VOLTAGE,
                                outputs.currentLimitA,
                                !isImdOk(),
                                get_latched_faults() != 0U,
                                outputs.enable);

    chargingStatus.phase = outputs.phase;
    chargingStatus.stopReason = outputs.stopReason;
    chargingStatus.commandEnable = outputs.enable;
    chargingStatus.commandVoltage = PACK_TARGET_VOLTAGE;
    chargingStatus.commandCurrent = outputs.currentLimitA;
    chargingStatus.chargerConnected = inputs.chargerConnected;
    chargingStatus.chargerVoltage = chargerVoltage;
    chargingStatus.chargerCurrent = chargerCurrent;
    chargingStatus.chargerEnabled = chargerEnabled;
}

void hvc_get_charging_status(hvc_charging_status_t *status) {
    *status = chargingStatus;
}
