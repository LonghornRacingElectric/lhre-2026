#include "vcu_model.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

// Calibration constants

// APPS calibration
#define APPS1_MIN_ADC 782u
#define APPS1_MAX_ADC 3262u
#define APPS2_MIN_ADC 382u
#define APPS2_MAX_ADC 1586u

// Torque limit (Nm)
#define TORQUE_MAX_NM 15.0f

// APPS plausibility
#define APPS_MIN_TRAVEL_FOR_CHECK 0.10f
#define APPS_MAX_DIFF_ALLOWED 0.10f
#define APPS_IMPLAUS_TIME_MS 100u
#define VCU_MODEL_STEP_MS 10u // matches actual 10 ms control loop
#define APPS_IMPLAUS_COUNT (APPS_IMPLAUS_TIME_MS / VCU_MODEL_STEP_MS)

// Pedal filter
#define PEDAL_FILTER_ALPHA 0.4f

// BSE calibration
#define BSE_ADC_AT_0_PSI 156.0f
#define BSE_ADC_AT_1000_PSI 635.0f
#define BSE_MAX_PSI 1000.0f

// Soft brake hysteresis
#define BSE_SOFT_ON_PSI 50.0f
#define BSE_SOFT_OFF_PSI 30.0f

// Internal state
typedef struct {
  float pedal_filtered;
  uint8_t implaus_counter;
  bool apps_implaus;

  bool brake_active;
  bool brake_latched;
} vcu_model_state_t;

static vcu_model_state_t s_state;

// Utility functions

static float clamp_f(float x, float lo, float hi) {
  return (x < lo) ? lo : (x > hi) ? hi : x;
}

static float apps_adc_to_travel(uint16_t raw, uint16_t min_adc,
                                uint16_t max_adc) {
  if (raw < min_adc)
    raw = min_adc;
  else if (raw > max_adc)
    raw = max_adc;

  float span = (float)(max_adc - min_adc);
  if (span <= 1.0f)
    return 0.0f;

  return ((float)raw - (float)min_adc) / span;
}

static float bse_adc_to_psi(uint16_t adc) {
  const float lo = BSE_ADC_AT_0_PSI;
  const float hi = BSE_ADC_AT_1000_PSI;

  float adc_f = clamp_f((float)adc, lo, hi); // clean float clamp
  return (adc_f - lo) / (hi - lo) * BSE_MAX_PSI;
}

// Public API

void vcu_model_init(void) {
  s_state.pedal_filtered = 0.0f;
  s_state.implaus_counter = 0;
  s_state.apps_implaus = false;
  s_state.brake_active = false;
  s_state.brake_latched = false;
}

void vcu_model_step(const vcu_inputs_t *in, vcu_outputs_t *out) {
  // APPS
  float p1 = apps_adc_to_travel(in->apps1_raw, APPS1_MIN_ADC, APPS1_MAX_ADC);
  float p2 = apps_adc_to_travel(in->apps2_raw, APPS2_MIN_ADC, APPS2_MAX_ADC);

  p1 = clamp_f(p1, 0.0f, 1.0f);
  p2 = clamp_f(p2, 0.0f, 1.0f);

  float diff = fabsf(p1 - p2);
  float p_max = fmaxf(p1, p2);

  // Plausibility detection
  if (p_max > APPS_MIN_TRAVEL_FOR_CHECK && diff > APPS_MAX_DIFF_ALLOWED)
    s_state.implaus_counter++;
  else
    s_state.implaus_counter = 0;

  if (s_state.implaus_counter >= APPS_IMPLAUS_COUNT)
    s_state.apps_implaus = true;

  // Reset implausibility if pedal returns to idle
  if (s_state.apps_implaus && p_max < 0.05f) {
    s_state.apps_implaus = false;
    s_state.implaus_counter = 0;
  }

  // Pedal value
  float pedal = (s_state.apps_implaus) ? 0.0f : 0.5f * (p1 + p2);
  pedal = clamp_f(pedal, 0.0f, 1.0f);

  // Filter
  s_state.pedal_filtered +=
      PEDAL_FILTER_ALPHA * (pedal - s_state.pedal_filtered);

  // BSE (Brake)
  float bse_psi = bse_adc_to_psi(in->bse_raw);

  if (!s_state.brake_active && bse_psi > BSE_SOFT_ON_PSI)
    s_state.brake_active = true;
  else if (s_state.brake_active && bse_psi < BSE_SOFT_OFF_PSI)
    s_state.brake_active = false;

  // Brake latch (brake + throttle)
  if (s_state.brake_active && s_state.pedal_filtered > 0.25f)
    s_state.brake_latched = true;

  if (s_state.brake_latched && s_state.pedal_filtered < 0.05f)
    s_state.brake_latched = false;

  // Torque Command
  float tq_cmd = 0.0f;
  if (!s_state.apps_implaus && !s_state.brake_latched)
    tq_cmd = s_state.pedal_filtered * TORQUE_MAX_NM;

  // Outputs
  out->apps1_travel = p1;
  out->apps2_travel = p2;
  out->pedal = pedal;
  out->pedal_filtered = s_state.pedal_filtered;

  out->torque_cmd = tq_cmd;

  out->apps_implaus = s_state.apps_implaus;
  out->apps_implaus_counter = s_state.implaus_counter;
  out->apps_diff = diff;

  out->brake_active = s_state.brake_active;
  out->brake_latched = s_state.brake_latched;

  out->bse_psi = bse_psi;
}
