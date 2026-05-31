#include "vcu_model.h"
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
  battery_init(&ctx->battery_state, &ctx->params);
  bse_init(&ctx->bse_state);
  cooling_init(&ctx->cooling_state, &ctx->params);
  torque_map_init(&ctx->params);
  power_limit_init(&ctx->power_limit_state, &ctx->params);
  regen_linelock_init(&ctx->regen_linelock_state, &ctx->params);
}

bool can_timed_out() { return false; }

bool any_fault_exists(vcu_outputs_t *out) {
  out->faults.any_fault =
      out->faults.apps_any_fault || out->faults.regen_linelock_any_fault;
  return out->faults.any_fault;
}

void vcu_model_step(vcu_model_context_t *ctx, const vcu_inputs_t *in,
                    vcu_outputs_t *out, uint32_t dt_ms) {
  ctx->time_ms += dt_ms;

  // default output state
  memset(out, 0, sizeof(vcu_outputs_t));

  battery_evaluate(in, out, &ctx->battery_state, &ctx->params, dt_ms);
  apps_evaluate(in, out, &ctx->apps_state, &ctx->params, dt_ms);
  bse_evaluate(in, out, &ctx->bse_state, &ctx->params, dt_ms);
  torque_map_evaluate(in, out, &ctx->params, dt_ms);
  power_limit_evaluate(in, out, &ctx->power_limit_state, &ctx->params, dt_ms);
  prndl_evaluate(&ctx->prndl_machine, in, out, &ctx->params, ctx->time_ms);
  regen_linelock_evaluate(in, out, &ctx->regen_linelock_state, &ctx->params,
                          dt_ms);
  cooling_evaluate(in, out, &ctx->cooling_state, &ctx->params, dt_ms);

  switch (out->prndl_state) {
  case PRNDL_DRIVE: {
    if (any_fault_exists(out)) {
      // disallow any torque output if there is any major fault detected
      out->torque_cmd = 0.0f;
      out->linelock_enabled = false;
      out->regen_torque_cmd_nm = 0.0f;

      // don't disable inverter here in case the fault fixes itself
      // let the state machine dictate inverter state
    }

    out->buzzer_active =
        (ctx->time_ms - ctx->prndl_machine.drive_start_time_ms <
         ctx->params.buzzer_duration_ms);
    out->inverter_enable = true;
    break;
  }

  case PRNDL_PARK:
  default:
    out->torque_cmd = 0.0f;
    out->linelock_enabled = false;
    out->regen_torque_cmd_nm = 0.0f;
    out->buzzer_active = false;
    out->inverter_enable = false;
    break;
  }
}
