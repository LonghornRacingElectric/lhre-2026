#include "charge_policy.h"

void charge_policy_init(charge_policy_t *policy)
{
    policy->chargeDone = false;
}

/* 1.0 below taperStart, ramping linearly to 0.0 at target. */
static float taperFraction(float cellV, float taperStart, float target)
{
    if (cellV <= taperStart) return 1.0f;
    if (cellV >= target) return 0.0f;
    return (target - cellV) / (target - taperStart);
}

static charge_stop_reason_t stopReason(const charge_inputs_t *inputs)
{
    /* Without valid cell data the taper below would see no high cell and
       command full current, so missing data is a stop, not a pass. */
    if (!inputs->cellDataValid || inputs->maxCellV <= 0.0f) {
        return CHARGE_STOP_NO_CELL_DATA;
    }
    if (inputs->bmsFault) return CHARGE_STOP_BMS_FAULT;
    if (!inputs->imdOk) return CHARGE_STOP_IMD;
    if (inputs->maxCellV > CHARGE_CELL_HARD_OV_V) return CHARGE_STOP_CELL_OV;
    if (inputs->maxTempC >= CHARGE_CUTOFF_TEMP_C) return CHARGE_STOP_OVERTEMP;
    return CHARGE_STOP_NONE;
}

charge_outputs_t charge_policy_update(charge_policy_t *policy,
                                      const charge_inputs_t *inputs)
{
    charge_outputs_t out = {
        .enable = false,
        .currentLimitA = 0.0f,
        .phase = CHARGE_PHASE_IDLE,
        .stopReason = CHARGE_STOP_NONE,
    };

    /* Unplugging re-arms a finished charge. */
    if (!inputs->chargerConnected) policy->chargeDone = false;

    /* Hard stops are evaluated even outside the charging state, so the
       command never carries enable while a fault is present. */
    out.stopReason = stopReason(inputs);
    if (out.stopReason != CHARGE_STOP_NONE) {
        out.phase = CHARGE_PHASE_STOPPED;
        return out;
    }

    /* Current only once the AIRs are closed (CHARGING), never during
       CHARGING_PRECHARGING. */
    if (!inputs->chargingState || !inputs->chargerConnected) return out;

    if (policy->chargeDone) {
        out.phase = CHARGE_PHASE_DONE;
        return out;
    }

    /* Done: the highest cell has reached the target and the charger's own
       current has fallen below the termination threshold. */
    if (inputs->maxCellV >= CHARGE_CELL_TARGET_V &&
        inputs->chargerCurrentA < CHARGE_TERMINATION_A) {
        policy->chargeDone = true;
        out.phase = CHARGE_PHASE_DONE;
        return out;
    }

    out.enable = true;
    out.currentLimitA = CHARGE_MAX_CURRENT_A *
        taperFraction(inputs->maxCellV, CHARGE_CELL_TAPER_START_V,
                      CHARGE_CELL_TARGET_V);
    out.phase = inputs->maxCellV <= CHARGE_CELL_TAPER_START_V
        ? CHARGE_PHASE_CC
        : CHARGE_PHASE_CV;
    return out;
}

const char *charge_policy_phase_name(charge_phase_t phase)
{
    switch (phase) {
      case CHARGE_PHASE_IDLE:    return "IDLE";
      case CHARGE_PHASE_CC:      return "CC";
      case CHARGE_PHASE_CV:      return "CV";
      case CHARGE_PHASE_DONE:    return "DONE";
      case CHARGE_PHASE_STOPPED: return "STOPPED";
      default:                   return "UNKNOWN";
    }
}

const char *charge_policy_stop_name(charge_stop_reason_t reason)
{
    switch (reason) {
      case CHARGE_STOP_NONE:         return "NONE";
      case CHARGE_STOP_NO_CELL_DATA: return "NO_CELL_DATA";
      case CHARGE_STOP_BMS_FAULT:    return "BMS_FAULT";
      case CHARGE_STOP_IMD:          return "IMD";
      case CHARGE_STOP_CELL_OV:      return "CELL_OV";
      case CHARGE_STOP_OVERTEMP:     return "OVERTEMP";
      default:                       return "UNKNOWN";
    }
}
