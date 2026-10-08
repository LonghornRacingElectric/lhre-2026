#include "charge_policy.h"

#include <assert.h>
#include <math.h>

static int near(float a, float b) { return fabsf(a - b) < 0.001f; }

static charge_inputs_t healthy(float maxCellV, float chargerCurrentA)
{
    charge_inputs_t in = {
        .chargingState = true,
        .chargerConnected = true,
        .cellDataValid = true,
        .maxCellV = maxCellV,
        .maxTempC = 30.0f,
        .bmsFault = false,
        .imdOk = true,
        .chargerCurrentA = chargerCurrentA,
    };
    return in;
}

int main(void)
{
    charge_policy_t policy;
    charge_policy_init(&policy);
    charge_inputs_t in;
    charge_outputs_t out;

    /* Not in the charging state yet (precharging): no current. */
    in = healthy(3.80f, 0.0f);
    in.chargingState = false;
    out = charge_policy_update(&policy, &in);
    assert(!out.enable && near(out.currentLimitA, 0.0f));
    assert(out.phase == CHARGE_PHASE_IDLE);

    /* CC: full current below the taper start. */
    in = healthy(3.80f, 9.0f);
    out = charge_policy_update(&policy, &in);
    assert(out.enable && near(out.currentLimitA, CHARGE_MAX_CURRENT_A));
    assert(out.phase == CHARGE_PHASE_CC);

    /* CV: linear taper, half way from 4.08 to 4.18 V gives half current. */
    in = healthy(4.13f, 4.0f);
    out = charge_policy_update(&policy, &in);
    assert(out.enable && near(out.currentLimitA, CHARGE_MAX_CURRENT_A * 0.5f));
    assert(out.phase == CHARGE_PHASE_CV);

    /* At target but the charger still pushing current: keep going at 0 A. */
    in = healthy(4.18f, 1.0f);
    out = charge_policy_update(&policy, &in);
    assert(out.enable && near(out.currentLimitA, 0.0f));

    /* At target and below termination current: done, and stays done. */
    in = healthy(4.18f, 0.3f);
    out = charge_policy_update(&policy, &in);
    assert(!out.enable && out.phase == CHARGE_PHASE_DONE);
    in = healthy(4.10f, 0.0f); /* cells relax after charge */
    out = charge_policy_update(&policy, &in);
    assert(!out.enable && out.phase == CHARGE_PHASE_DONE);

    /* Unplug re-arms; replug charges again. */
    in = healthy(4.10f, 0.0f);
    in.chargerConnected = false;
    out = charge_policy_update(&policy, &in);
    assert(!out.enable && out.phase == CHARGE_PHASE_IDLE);
    in = healthy(4.10f, 0.0f);
    out = charge_policy_update(&policy, &in);
    assert(out.enable && out.phase == CHARGE_PHASE_CV);

    /* Hard stops, each forcing enable off and 0 A. */
    in = healthy(4.21f, 2.0f);
    out = charge_policy_update(&policy, &in);
    assert(!out.enable && out.stopReason == CHARGE_STOP_CELL_OV);

    in = healthy(3.90f, 5.0f);
    in.maxTempC = 55.0f;
    out = charge_policy_update(&policy, &in);
    assert(!out.enable && out.stopReason == CHARGE_STOP_OVERTEMP);

    in = healthy(3.90f, 5.0f);
    in.bmsFault = true;
    out = charge_policy_update(&policy, &in);
    assert(!out.enable && out.stopReason == CHARGE_STOP_BMS_FAULT);

    in = healthy(3.90f, 5.0f);
    in.imdOk = false;
    out = charge_policy_update(&policy, &in);
    assert(!out.enable && out.stopReason == CHARGE_STOP_IMD);

    /* Missing cell data (max cell -999) must stop, not run at full current. */
    in = healthy(-999.0f, 0.0f);
    out = charge_policy_update(&policy, &in);
    assert(!out.enable && near(out.currentLimitA, 0.0f));
    assert(out.stopReason == CHARGE_STOP_NO_CELL_DATA);
    in = healthy(3.90f, 0.0f);
    in.cellDataValid = false;
    out = charge_policy_update(&policy, &in);
    assert(!out.enable && out.stopReason == CHARGE_STOP_NO_CELL_DATA);

    /* A stop is reported even outside the charging state. */
    in = healthy(4.25f, 0.0f);
    in.chargingState = false;
    out = charge_policy_update(&policy, &in);
    assert(!out.enable && out.phase == CHARGE_PHASE_STOPPED);
    return 0;
}
