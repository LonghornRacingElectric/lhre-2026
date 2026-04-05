#include "vcu_model.h"
#include "TorqueMap.h"
#include "string.h"
#include "util.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <vcu_outputs.h>

// Public API

void vcu_model_init(vcu_model_context_t *ctx, const vcu_parameters_t *params) {
  // copy params -- we do a copy because we only want this to be updatable
  // at init or when in park, which is dictated by the model's state machine,
  // not by firmware
  ctx->params = *params;
  ctx->time_ms = 0;

  prndl_init(&ctx->prndl_machine);
  apps_init(&ctx->apps_state);
  bse_init(&ctx->bse_state);
  cooling_init(&ctx->cooling_state, &ctx->params);
}

bool can_timed_out() { return false; }

bool any_fault_exists(vcu_outputs_t *out) {
  out->faults.any_fault =
      out->faults.apps_any_fault || out->faults.brake_any_fault;
  return out->faults.any_fault;
}

void vcu_model_step(vcu_model_context_t *ctx, const vcu_inputs_t *in,
                    vcu_outputs_t *out, uint32_t dt_ms) {
  // update time
  ctx->time_ms += dt_ms;

  // default output state
  memset(out, 0, sizeof(vcu_outputs_t));

  // Evaluate sensors and check APPS plausibility
  apps_evaluate(in, out, &ctx->apps_state, &ctx->params, dt_ms);

  // check BSE
  bse_evaluate(in, out, &ctx->bse_state, &ctx->params, dt_ms);

  // perform mapping from pedal to output
  torque_map_evaluate(in, out, &ctx->params, dt_ms);

  // get the state for this step
  prndl_evaluate(&ctx->prndl_machine, in, out, ctx->time_ms);

  // evaluate cooling
  cooling_evaluate(in, out, &ctx->cooling_state, &ctx->params, dt_ms);


  // Latch outputs based on current state
  switch (out->prndl_state) {
  case PRNDL_DRIVE: {
    if (any_fault_exists(out)) {
      // disallow any torque output if there is any major fault detected
      out->torque_cmd = 0.0f;

      // don't disable inverter here in case the fault fixes itself
      // let the state machine dictate inverter state
    }

    // buzzer only on for the first 3 seconds of drive
    out->buzzer_active =
        (ctx->time_ms - ctx->prndl_machine.drive_start_time_ms <
         ctx->params.buzzer_duration_ms);
    out->inverter_enable = true;
    break;
  }

  case PRNDL_PARK:
  default:
    out->torque_cmd = 0.0f;
    out->buzzer_active = false;
    out->inverter_enable = false;
    break;
  }
}