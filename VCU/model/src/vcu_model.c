#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include "vcu_model.h"
/* ---------- Calibration & constants (moved from firmware) ---------- */

/* APPS ADC calibration (from your firmware) */
#define APPS1_MIN_ADC  782u
#define APPS1_MAX_ADC  3262u
#define APPS2_MIN_ADC  382u
#define APPS2_MAX_ADC  1586u

/* Torque command limit */
#define TORQUE_MAX_NM  5.0f

/* APPS plausibility */
#define APPS_MIN_TRAVEL_FOR_CHECK  0.10f
#define APPS_MAX_DIFF_ALLOWED      0.10f   /* in normalized units */
#define APPS_IMPLAUS_TIME_MS       100u

/* Control step period (must match how often you call vcu_model_step) */
#define VCU_MODEL_STEP_MS          50u
#define APPS_IMPLAUS_COUNT         (APPS_IMPLAUS_TIME_MS / VCU_MODEL_STEP_MS)

/* Pedal filtering */
#define PEDAL_FILTER_ALPHA         0.4f

/* BSE threshold (raw ADC) */
#define BSE_ACTIVE_ADC             1500u   /* brake considered active above this */

/* ---------- Internal state (persists across steps) ---------- */

typedef struct
{
    float pedal_filtered;
    uint8_t implaus_counter;
    bool apps_implaus;
    bool brake_latched;
} vcu_model_state_t;

static vcu_model_state_t s_state;

/* ---------- Helper: clamp ---------- */
static float clamp_f(float x, float lo, float hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

/* ---------- Helper: convert ADC -> normalized travel ---------- */
static float apps_adc_to_travel(uint16_t raw, uint16_t min_adc, uint16_t max_adc)
{
    if (raw < min_adc) raw = min_adc;
    if (raw > max_adc) raw = max_adc;

    float span = (float)(max_adc - min_adc);
    if (span <= 1.0f)
        return 0.0f;

    return ((float)raw - (float)min_adc) / span;
}

/* ---------- Public API ---------- */

void vcu_model_init(void)
{
    s_state.pedal_filtered  = 0.0f;
    s_state.implaus_counter = 0;
    s_state.apps_implaus    = false;
    s_state.brake_latched   = false;
}

void vcu_model_step(const vcu_inputs_t *in, vcu_outputs_t *out)
{
    /* ---- 1. Normalize APPS sensors ---- */
    float p1 = apps_adc_to_travel(in->apps1_raw, APPS1_MIN_ADC, APPS1_MAX_ADC);
    float p2 = apps_adc_to_travel(in->apps2_raw, APPS2_MIN_ADC, APPS2_MAX_ADC);

    p1 = clamp_f(p1, 0.0f, 1.0f);
    p2 = clamp_f(p2, 0.0f, 1.0f);

    /* ---- 2. APPS plausibility check ---- */
    float p_max = (p1 > p2) ? p1 : p2;
    float diff  = fabsf(p1 - p2);

    if (p_max > APPS_MIN_TRAVEL_FOR_CHECK) {
        if (diff > APPS_MAX_DIFF_ALLOWED) {
            if (s_state.implaus_counter < 255)
                s_state.implaus_counter++;
        } else {
            s_state.implaus_counter = 0;
        }
    } else {
        s_state.implaus_counter = 0;
    }

    if (s_state.implaus_counter >= APPS_IMPLAUS_COUNT) {
        s_state.apps_implaus = true;
    }

    /* Clear implausibility when both pedals low */
    if (s_state.apps_implaus && p1 < 0.05f && p2 < 0.05f) {
        s_state.apps_implaus    = false;
        s_state.implaus_counter = 0;
    }

    /* ---- 3. Compute combined pedal and filter ---- */
    float pedal = 0.0f;
    if (!s_state.apps_implaus) {
        pedal = 0.5f * (p1 + p2);
    }
    pedal = clamp_f(pedal, 0.0f, 1.0f);

    s_state.pedal_filtered += PEDAL_FILTER_ALPHA * (pedal - s_state.pedal_filtered);

    /* ---- 4. BSE logic & brake-throttle override ---- */
    bool brake_active = (in->bse_raw > BSE_ACTIVE_ADC);

    /* Latch brake override when brake pressed AND throttle > 25% */
    if (brake_active && s_state.pedal_filtered > 0.25f) {
        s_state.brake_latched = true;
    }

    /* Clear latch when pedal nearly released */
    if (s_state.brake_latched && s_state.pedal_filtered < 0.05f) {
        s_state.brake_latched = false;
    }

    /* ---- 5. Torque command ---- */
    float tq_cmd = 0.0f;
    if (!s_state.apps_implaus && !s_state.brake_latched) {
        tq_cmd = s_state.pedal_filtered * TORQUE_MAX_NM;
    }

    /* ---- 6. Fill outputs ---- */
    out->apps1_travel        = p1;
    out->apps2_travel        = p2;
    out->pedal               = pedal;
    out->pedal_filtered      = s_state.pedal_filtered;
    out->torque_cmd          = tq_cmd;

    out->apps_implaus        = s_state.apps_implaus;
    out->brake_active        = brake_active;
    out->brake_latched       = s_state.brake_latched;

    out->apps_implaus_counter = s_state.implaus_counter;
    out->apps_diff            = diff;
}
