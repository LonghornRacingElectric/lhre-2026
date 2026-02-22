#include "vcu_model.h"
#include "APPS.h"
#include "TorqueMap.h"
#include "util.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

// Internal state
typedef struct {
  float pedal_filtered;
  uint8_t implaus_counter;
  bool apps_implaus;

  bool brake_active;
  bool brake_latched;

  bool drive_switch;
  uint32_t drive_start_time_ms;
} vcu_model_state_t;

static vcu_model_state_t s_prev_state = {0};
static prndl_state_t s_prndl_state = PRNDL_PARK;
static uint32_t s_time_ms = 0;
static vcu_parameters_t s_params = {0};

// Public API

void vcu_model_init(vcu_parameters_t *params) {
  // copy params -- we do a copy because we only want this to be updatable
  // at init or when in park, which is dictated by the model's state machine,
  // not by firmware
  s_params = *params;
  s_prev_state.pedal_filtered = 0.0f;
  s_prev_state.implaus_counter = 0;
  s_prev_state.apps_implaus = false;
  s_prev_state.brake_active = false;
  s_prev_state.brake_latched = false;
  s_prev_state.drive_start_time_ms = 0;
  s_prndl_state = PRNDL_PARK;
}

bool can_timed_out() { return true; }
bool brake_threshold_reached(const vcu_inputs_t *in) { return false; }

void update_state_machine(const vcu_inputs_t *in) {
  switch (s_prndl_state) {
  case PRNDL_PARK:
    // Transition to DRIVE on (rising edge of switch) + brake + contactors
    if (rising_edge(s_prev_state.drive_switch, in->drive_switch) &&
        in->contactors_closed && brake_threshold_reached(in)) {
      s_prndl_state = PRNDL_DRIVE;
      s_prev_state.drive_start_time_ms = s_time_ms;
    }
    break;
  case PRNDL_DRIVE:
    // Return to PARK if switch opened or contactors drop
    if (!in->drive_switch || !in->contactors_closed) {
      s_prndl_state = PRNDL_PARK;
    }
    break;
  }

  s_prev_state.drive_switch = in->drive_switch;
}

void vcu_model_step(const vcu_inputs_t *in, vcu_outputs_t *out,
                    uint32_t dt_ms) {
  // update time
  s_time_ms += dt_ms;

  // get the state for this step
  update_state_machine(in);

  switch (s_prndl_state) {
  case PRNDL_PARK:
    out->torque_cmd = 0.0f;
    out->buzzer_active = false;
    break;
  case PRNDL_DRIVE: {
    apps_evaluate(in, out, &s_params, dt_ms);

    if (out->apps_implaus) {
      // disallow any torque output if the apps sensor detects an implausibility
      out->torque_cmd = 0.0f;
    } else {
      torque_map_evaluate(in, out, &s_params, dt_ms);
    }

    // buzzer only on for the first 3 seconds of drive
    out->buzzer_active = (s_time_ms - s_prev_state.drive_start_time_ms <
                          s_params.buzzer_duration_ms);
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
