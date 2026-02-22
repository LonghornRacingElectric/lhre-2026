#include "vcu_model.h"
#include "APPS.h"
#include "BSE.h"
#include "TorqueMap.h"
#include "util.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

// Public API

void vcu_model_init(vcu_model_context_t *ctx, const vcu_parameters_t *params) {
  // copy params -- we do a copy because we only want this to be updatable
  // at init or when in park, which is dictated by the model's state machine,
  // not by firmware
  ctx->params = *params;
  ctx->prev_state.drive_start_time_ms = 0;
  ctx->prev_state.drive_switch = false;
  ctx->prndl_state = PRNDL_PARK;
  ctx->time_ms = 0;

  apps_init(&ctx->apps_state);
  bse_init(&ctx->bse_state);
}

bool can_timed_out() { return false; }
bool brake_threshold_reached(vcu_model_context_t *ctx, const vcu_inputs_t *in) {
  return bse_is_active(bse_adc_to_psi(in->bse_raw, &ctx->params),
                       &ctx->bse_state, &ctx->params);
}

void update_state_machine(vcu_model_context_t *ctx, const vcu_inputs_t *in) {
  switch (ctx->prndl_state) {
  case PRNDL_PARK:
    // Transition to DRIVE on (rising edge of switch) + brake + contactors
    if (rising_edge(ctx->prev_state.drive_switch, in->drive_switch) &&
        in->contactors_closed && brake_threshold_reached(ctx, in)) {
      ctx->prndl_state = PRNDL_DRIVE;
      ctx->prev_state.drive_start_time_ms = ctx->time_ms;
    }
    break;
  case PRNDL_DRIVE:
    // Return to PARK if switch opened or contactors drop
    if (!in->drive_switch || !in->contactors_closed) {
      ctx->prndl_state = PRNDL_PARK;
    }
    break;
  }

  ctx->prev_state.drive_switch = in->drive_switch;
}

void vcu_model_step(vcu_model_context_t *ctx, const vcu_inputs_t *in,
                    vcu_outputs_t *out, uint32_t dt_ms) {
  // update time
  ctx->time_ms += dt_ms;

  // get the state for this step
  update_state_machine(ctx, in);

  switch (ctx->prndl_state) {
  case PRNDL_PARK:
    out->torque_cmd = 0.0f;
    out->buzzer_active = false;
    break;
  case PRNDL_DRIVE: {
    // Evaluate sensors and check APPS plausibility
    apps_evaluate(in, out, &ctx->apps_state, &ctx->params, dt_ms);

    // TODO: implement a proper filtering algorithm
    // set pedal to the average of the two sensors
    out->pedal = linear_interp(out->apps1_travel, out->apps2_travel, 0.5f);

    // set pedal filtered to the average of the two sensors
    out->pedal_filtered =
        linear_interp(out->apps1_travel, out->apps2_travel, 0.5f);

    bse_evaluate(in, out, &ctx->bse_state, &ctx->params, dt_ms);

    if (out->apps_implaus || out->brake_latched) {
      // disallow any torque output if the apps sensor detects an implausibility
      // or the brake is latched
      out->torque_cmd = 0.0f;
    } else {
      torque_map_evaluate(in, out, &ctx->params, dt_ms);
    }

    // buzzer only on for the first 3 seconds of drive
    out->buzzer_active = (ctx->time_ms - ctx->prev_state.drive_start_time_ms <
                          ctx->params.buzzer_duration_ms);
    break;
  }
  }
}

/**
old */

// static float clamp_f(float x, float lo, float hi) {
//   return (x < lo) ? lo : (x > hi) ? hi : x;
// }

// static float apps_adc_to_travel(uint16_t raw, uint16_t min_adc,
//                                 uint16_t max_adc) {
//   if (raw < min_adc)
//     raw = min_adc;
//   else if (raw > max_adc)
//     raw = max_adc;

//   float span = (float)(max_adc - min_adc);
//   if (span <= 1.0f)
//     return 0.0f;

//   return ((float)raw - (float)min_adc) / span;
// }

// static float bse_adc_to_psi(uint16_t adc) {
//   const float lo = BSE_ADC_AT_0_PSI;
//   const float hi = BSE_ADC_AT_1000_PSI;

//   float adc_f = clamp_f((float)adc, lo, hi); // clean float clamp
//   return (adc_f - lo) / (hi - lo) * BSE_MAX_PSI;
// }
