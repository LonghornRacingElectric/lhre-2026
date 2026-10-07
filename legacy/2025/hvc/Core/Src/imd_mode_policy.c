#include "imd_mode_policy.h"

static void enterState(imd_mode_policy_t *policy, imd_state_t state,
                       uint32_t nowMs)
{
    policy->state = state;
    policy->stateSinceMs = nowMs;
    policy->okSeen = false;
    policy->faultSeen = false;
}

static void trip(imd_mode_policy_t *policy, imd_trip_reason_t reason,
                 uint32_t nowMs)
{
    enterState(policy, IMD_STATE_TRIPPED, nowMs);
    policy->tripReason = reason;
}

void imd_mode_policy_init(imd_mode_policy_t *policy, uint32_t nowMs)
{
    policy->tripReason = IMD_TRIP_NONE;
    enterState(policy, IMD_STATE_DISARMED, nowMs);
}

imd_state_t imd_mode_policy_update(imd_mode_policy_t *policy,
                                   bool airsClosed,
                                   bool imdOk,
                                   uint32_t nowMs)
{
    if (policy->state == IMD_STATE_TRIPPED) return policy->state;

    /* AIRs open (key-off, or another shutdown device): the iso175 is about to
       see 0 V, so stop judging it. A real IMD fault cannot open the AIRs by
       itself any more; only a trip here sets the latch. */
    if (!airsClosed) {
        if (policy->state != IMD_STATE_DISARMED) {
            enterState(policy, IMD_STATE_DISARMED, nowMs);
        }
        return policy->state;
    }

    switch (policy->state) {
      case IMD_STATE_DISARMED:
        enterState(policy, IMD_STATE_WAIT_OK, nowMs);
        /* fall through: judge the first sample right away */
      case IMD_STATE_WAIT_OK:
        if (imdOk) {
            if (!policy->okSeen) {
                policy->okSeen = true;
                policy->okSinceMs = nowMs;
            }
            if ((uint32_t)(nowMs - policy->okSinceMs) >= IMD_OK_QUALIFY_MS) {
                enterState(policy, IMD_STATE_ARMED, nowMs);
                break;
            }
        } else {
            policy->okSeen = false;
        }
        if ((uint32_t)(nowMs - policy->stateSinceMs) >= IMD_OK_TIMEOUT_MS) {
            trip(policy, IMD_TRIP_TIMEOUT, nowMs);
        }
        break;

      case IMD_STATE_ARMED:
        if (imdOk) {
            policy->faultSeen = false;
            break;
        }
        if (!policy->faultSeen) {
            policy->faultSeen = true;
            policy->faultSinceMs = nowMs;
        }
        if ((uint32_t)(nowMs - policy->faultSinceMs) >= IMD_FAULT_DEBOUNCE_MS) {
            trip(policy, IMD_TRIP_FAULT, nowMs);
        }
        break;

      case IMD_STATE_TRIPPED:
      default:
        break;
    }
    return policy->state;
}

bool imd_mode_policy_tripped(const imd_mode_policy_t *policy)
{
    return policy->state == IMD_STATE_TRIPPED;
}

const char *imd_mode_policy_state_name(imd_state_t state)
{
    switch (state) {
      case IMD_STATE_DISARMED: return "DISARMED";
      case IMD_STATE_WAIT_OK:  return "WAIT_OK";
      case IMD_STATE_ARMED:    return "ARMED";
      case IMD_STATE_TRIPPED:  return "TRIPPED";
      default:                 return "UNKNOWN";
    }
}

const char *imd_mode_policy_trip_name(imd_trip_reason_t reason)
{
    switch (reason) {
      case IMD_TRIP_NONE:    return "NONE";
      case IMD_TRIP_FAULT:   return "FAULT";
      case IMD_TRIP_TIMEOUT: return "NO_OK_TIMEOUT";
      default:               return "UNKNOWN";
    }
}
