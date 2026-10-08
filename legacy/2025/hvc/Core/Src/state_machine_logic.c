#include "state_machine_logic.h"

static void updateOutputs(const hvc_state_machine_context_t *context,
                          hvc_state_machine_outputs_t *outputs) {
    outputs->positiveContactorClosed =
        context->state == HVC_STATE_ENERGIZED ||
        context->state == HVC_STATE_CHARGING;
    outputs->chargerEnabled = context->state == HVC_STATE_CHARGING;
}

void hvc_state_machine_reset(hvc_state_machine_context_t *context,
                             hvc_state_machine_outputs_t *outputs) {
    context->state = HVC_STATE_NOT_ENERGIZED;
    context->prechargeStartTimeMs = 0U;
    updateOutputs(context, outputs);
}

void hvc_state_machine_step(hvc_state_machine_context_t *context,
                            const hvc_state_machine_inputs_t *inputs,
                            hvc_state_machine_outputs_t *outputs) {
    if (inputs->anyFaults) {
        context->state = HVC_STATE_NOT_ENERGIZED;
        updateOutputs(context, outputs);
        return;
    }

    const float prechargeThreshold =
        inputs->packVoltage * HVC_PRECHARGE_THRESHOLD_PERCENT;

    switch (context->state) {
      case HVC_STATE_NOT_ENERGIZED:
        if (inputs->shutdownClosed) {
            context->prechargeStartTimeMs = inputs->currentTimeMs;
            context->state = inputs->chargerConnected
                ? HVC_STATE_CHARGING_PRECHARGING
                : HVC_STATE_PRECHARGING;
        }
        break;

      case HVC_STATE_PRECHARGING:
        if (inputs->tractiveVoltage > prechargeThreshold) {
            if ((uint32_t)(inputs->currentTimeMs -
                           context->prechargeStartTimeMs) >=
                HVC_PRECHARGE_VALID_MS) {
                context->state = HVC_STATE_ENERGIZED;
            }
        } else {
            context->prechargeStartTimeMs = inputs->currentTimeMs;
        }

        if (!inputs->shutdownClosed) {
            context->state = HVC_STATE_NOT_ENERGIZED;
        }
        break;

      case HVC_STATE_ENERGIZED:
        if (!inputs->shutdownClosed) {
            context->state = HVC_STATE_NOT_ENERGIZED;
        }
        break;

      case HVC_STATE_CHARGING_PRECHARGING:
        if (inputs->tractiveVoltage > prechargeThreshold) {
            if ((uint32_t)(inputs->currentTimeMs -
                           context->prechargeStartTimeMs) >=
                HVC_PRECHARGE_VALID_MS) {
                context->state = HVC_STATE_CHARGING;
            }
        } else {
            context->prechargeStartTimeMs = inputs->currentTimeMs;
        }

        if (!inputs->chargerConnected || !inputs->shutdownClosed) {
            context->state = HVC_STATE_NOT_ENERGIZED;
        }
        break;

      case HVC_STATE_CHARGING:
        if (!inputs->chargerConnected || !inputs->shutdownClosed) {
            context->state = HVC_STATE_NOT_ENERGIZED;
        }
        break;

      default:
        context->state = HVC_STATE_NOT_ENERGIZED;
        break;
    }

    updateOutputs(context, outputs);
}
