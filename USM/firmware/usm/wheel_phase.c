#include "wheel_phase.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define WP_PI 3.14159265358979323846f
#define WP_TWO_PI (2.0f * WP_PI)
#define WP_POLE_PAIRS 3.0f
#define WP_HALF_TIME_RANGE UINT32_C(0x80000000)

static float wrap_phase(float value) {
  value = fmodf(value + WP_PI, WP_TWO_PI);
  if (value < 0.0f) value += WP_TWO_PI;
  return value - WP_PI;
}

static bool positive_finite(float value) {
  return isfinite(value) && value > 0.0f;
}

static bool config_valid(const wheel_phase_config_t *c) {
  if (c == NULL) return false;
  for (unsigned i = 0; i < WHEEL_PHASE_CHANNELS; ++i) {
    if (!isfinite(c->offset[i]) || !positive_finite(c->scale[i]) ||
        (c->sign[i] != 1 && c->sign[i] != -1)) return false;
  }
  return isfinite(c->pair1_correction_rad) &&
         fabsf(c->pair1_correction_rad) <= WP_PI &&
         positive_finite(c->min_magnitude) &&
         positive_finite(c->max_magnitude) &&
         c->max_magnitude > c->min_magnitude &&
         positive_finite(c->max_pair_disagreement_rad) &&
         c->max_pair_disagreement_rad < WP_PI / 2.0f &&
         positive_finite(c->phase_noise_bound_rad) &&
         c->phase_noise_bound_rad < WP_PI / 4.0f &&
         positive_finite(c->max_speed_rad_s) &&
         c->max_speed_rad_s <= 10000.0f &&
         positive_finite(c->max_innovation_rad) &&
         c->max_innovation_rad < WP_PI &&
         positive_finite(c->alpha) && c->alpha <= 1.0f &&
         positive_finite(c->beta) && c->beta < (4.0f - 2.0f * c->alpha) &&
         c->min_dt_us > 0 && c->max_gap_us >= c->min_dt_us &&
         c->max_gap_us < WP_HALF_TIME_RANGE &&
         c->max_sample_age_us > 0 &&
         c->max_sample_age_us < WP_HALF_TIME_RANGE &&
         c->max_channel_skew_us <= c->max_sample_age_us &&
         c->acquire_time_us >= c->min_dt_us &&
         c->acquire_time_us < WP_HALF_TIME_RANGE && c->acquire_samples >= 3;
}

wheel_phase_config_t wheel_phase_default_config(void) {
  wheel_phase_config_t c = {0};
  for (unsigned i = 0; i < WHEEL_PHASE_CHANNELS; ++i) {
    c.scale[i] = 1.0f;
    c.sign[i] = 1;
  }
  c.pair1_correction_rad = WP_PI / 4.0f;
  c.min_magnitude = 0.5f;
  c.max_magnitude = 1.5f;
  c.max_pair_disagreement_rad = 0.15f;
  c.phase_noise_bound_rad = 0.03f;
  c.max_speed_rad_s = 250.0f;
  c.max_innovation_rad = 0.5f;
  c.alpha = 0.35f;
  c.beta = 0.04f;
  c.min_dt_us = 100;
  c.max_gap_us = 3000;
  c.max_sample_age_us = 2000;
  c.max_channel_skew_us = 100;
  c.acquire_time_us = 10000;
  c.acquire_samples = 8;
  return c;
}

void wheel_phase_reset(wheel_phase_t *s) {
  if (s == NULL) return;
  const wheel_phase_config_t c = s->config;
  const bool configured = s->configured;
  memset(s, 0, sizeof(*s));
  s->config = c;
  s->configured = configured;
  s->output.status = configured ? WHEEL_PHASE_ACQUIRING : WHEEL_PHASE_BAD_CONFIG;
  if (configured && !c.calibrated) s->output.status = WHEEL_PHASE_UNCALIBRATED;
}

bool wheel_phase_init(wheel_phase_t *s, const wheel_phase_config_t *c) {
  if (s == NULL) return false;
  /* Copy first to allow reinitializing from &state->config. */
  wheel_phase_config_t copy = {0};
  if (c != NULL) copy = *c;
  memset(s, 0, sizeof(*s));
  s->config = copy;
  s->configured = config_valid(c == NULL ? NULL : &copy);
  wheel_phase_reset(s);
  return s->configured;
}

static bool reject(wheel_phase_t *s, uint32_t status) {
  s->tracking = false;
  s->accepted_samples = 0;
  s->electrical_speed_rad_s = 0.0f;
  s->relative_electrical_angle_rad = 0.0;
  memset(&s->output, 0, sizeof(s->output));
  s->output.status = status;
  return false;
}

bool wheel_phase_update(wheel_phase_t *s, const wheel_phase_sample_t *sample,
                        uint32_t now_us) {
  if (s == NULL) return false;
  if (!s->configured || !config_valid(&s->config))
    return reject(s, WHEEL_PHASE_BAD_CONFIG);
  const wheel_phase_config_t *c = &s->config;
  if (!c->calibrated) return reject(s, WHEEL_PHASE_UNCALIBRATED);
  if (sample == NULL || sample->valid_mask != WHEEL_PHASE_ALL_CHANNELS)
    return reject(s, WHEEL_PHASE_BAD_SAMPLE);
  if (sample->fresh_mask != WHEEL_PHASE_ALL_CHANNELS)
    return reject(s, WHEEL_PHASE_STALE);

  float normalized[WHEEL_PHASE_CHANNELS];
  uint32_t ages[WHEEL_PHASE_CHANNELS];
  uint32_t min_age = UINT32_MAX;
  uint32_t max_age = 0;
  for (unsigned i = 0; i < WHEEL_PHASE_CHANNELS; ++i) {
    if (!isfinite(sample->radial[i])) return reject(s, WHEEL_PHASE_BAD_SAMPLE);
    normalized[i] = ((sample->radial[i] - c->offset[i]) / c->scale[i]) *
                    (float)c->sign[i];
    if (!isfinite(normalized[i])) return reject(s, WHEEL_PHASE_BAD_SAMPLE);
    ages[i] = now_us - sample->sample_time_us[i];
    if (ages[i] >= WP_HALF_TIME_RANGE) return reject(s, WHEEL_PHASE_BAD_TIME);
    if (ages[i] > c->max_sample_age_us) return reject(s, WHEEL_PHASE_STALE);
    if (s->have_channel_times) {
      const uint32_t advance = sample->sample_time_us[i] - s->last_channel_time_us[i];
      if (advance == 0) return reject(s, WHEEL_PHASE_STALE);
      if (advance >= WP_HALF_TIME_RANGE) return reject(s, WHEEL_PHASE_BAD_TIME);
    }
    if (ages[i] < min_age) min_age = ages[i];
    if (ages[i] > max_age) max_age = ages[i];
  }
  if (max_age - min_age > c->max_channel_skew_us)
    return reject(s, WHEEL_PHASE_SKEW);

  /* Timestamp the fused observation at the newest channel. Pair phases are
   * predicted from pair midpoint to this time using the preceding estimate.
   * This does NOT remove intra-pair skew distortion; the skew bound remains
   * mandatory and simultaneous conversion is preferable. */
  const uint32_t time_us = now_us - min_age;
  const uint32_t dt_us = time_us - s->last_time_us;
  if (s->tracking && (dt_us >= WP_HALF_TIME_RANGE || dt_us < c->min_dt_us))
    return reject(s, WHEEL_PHASE_BAD_TIME);
  if (s->tracking && dt_us > c->max_gap_us) return reject(s, WHEEL_PHASE_GAP);
  const float dt = (float)dt_us * 1.0e-6f;
  /* Refuse to unwrap when the configured speed envelope permits > pi travel.
   * No estimator can discover unobserved complete electrical revolutions. */
  const float phase_step_bound = c->max_speed_rad_s * WP_POLE_PAIRS * dt +
                                 2.0f * c->phase_noise_bound_rad;
  if (s->tracking && phase_step_bound >= WP_PI) return reject(s, WHEEL_PHASE_ALIAS);

  const float magnitude0 = hypotf(normalized[0], normalized[2]);
  const float magnitude1 = hypotf(normalized[1], normalized[3]);
  if (!isfinite(magnitude0) || !isfinite(magnitude1) ||
      magnitude0 < c->min_magnitude || magnitude0 > c->max_magnitude ||
      magnitude1 < c->min_magnitude || magnitude1 > c->max_magnitude)
    return reject(s, WHEEL_PHASE_BAD_MAGNITUDE);

  float phase0 = atan2f(normalized[2], normalized[0]);
  float phase1 = atan2f(normalized[3], normalized[1]) + c->pair1_correction_rad;
  if (s->tracking && s->accepted_samples >= 2) {
    /* Convert each small age individually before averaging to avoid overflow. */
    const float age0 = 0.5f * (float)(ages[0] - min_age) +
                       0.5f * (float)(ages[2] - min_age);
    const float age1 = 0.5f * (float)(ages[1] - min_age) +
                       0.5f * (float)(ages[3] - min_age);
    phase0 += s->electrical_speed_rad_s * age0 * 1.0e-6f;
    phase1 += s->electrical_speed_rad_s * age1 * 1.0e-6f;
  }
  const float disagreement = wrap_phase(phase1 - phase0);
  if (fabsf(disagreement) > c->max_pair_disagreement_rad)
    return reject(s, WHEEL_PHASE_PAIR_DISAGREEMENT);
  const float measured = wrap_phase(phase0 + 0.5f * disagreement);
  float innovation = 0.0f;

  if (!s->tracking) {
    s->tracking = true;
    s->acquisition_start_us = time_us;
    s->accepted_samples = 1;
    s->estimated_phase_rad = measured;
    s->electrical_speed_rad_s = 0.0f;
    s->relative_electrical_angle_rad = 0.0;
  } else {
    const float measured_step = wrap_phase(measured - s->last_measured_phase_rad);
    if (fabsf(measured_step) > phase_step_bound) return reject(s, WHEEL_PHASE_ALIAS);
    float estimate_step;
    if (s->accepted_samples == 1) {
      /* Bootstrap direction and speed; acquisition interval keeps this noisy
       * two-point estimate out of the valid output. */
      s->electrical_speed_rad_s = measured_step / dt;
      s->estimated_phase_rad = measured;
      estimate_step = measured_step;
    } else {
      const float prediction = wrap_phase(s->estimated_phase_rad +
                                           s->electrical_speed_rad_s * dt);
      innovation = wrap_phase(measured - prediction);
      if (fabsf(innovation) > c->max_innovation_rad)
        return reject(s, WHEEL_PHASE_INNOVATION);
      estimate_step = s->electrical_speed_rad_s * dt + c->alpha * innovation;
      s->estimated_phase_rad = wrap_phase(prediction + c->alpha * innovation);
      s->electrical_speed_rad_s += c->beta * innovation / dt;
    }
    if (!isfinite(s->electrical_speed_rad_s) ||
        fabsf(s->electrical_speed_rad_s) > c->max_speed_rad_s * WP_POLE_PAIRS)
      return reject(s, WHEEL_PHASE_ALIAS);
    s->relative_electrical_angle_rad += (double)estimate_step;
    if (s->accepted_samples < UINT16_MAX) ++s->accepted_samples;
  }

  s->last_measured_phase_rad = measured;
  s->last_time_us = time_us;
  for (unsigned i = 0; i < WHEEL_PHASE_CHANNELS; ++i)
    s->last_channel_time_us[i] = sample->sample_time_us[i];
  s->have_channel_times = true;
  const uint32_t acquisition_age = time_us - s->acquisition_start_us;
  /* Only gate by duration until acquired; otherwise the 32-bit clock wrapping
   * after a long continuous run could spuriously remove validity. */
  const bool acquired = s->output.valid ||
      (s->accepted_samples >= c->acquire_samples && acquisition_age >= c->acquire_time_us);
  s->output.valid = acquired;
  s->output.phase_valid = true;
  s->output.status = acquired ? 0 : WHEEL_PHASE_ACQUIRING;
  s->output.sample_time_us = time_us;
  s->output.speed_rad_s = acquired ? s->electrical_speed_rad_s / WP_POLE_PAIRS : 0.0f;
  s->output.electrical_phase_rad = s->estimated_phase_rad;
  s->output.relative_angle_rad = s->relative_electrical_angle_rad / WP_POLE_PAIRS;
  s->output.pair_disagreement_rad = fabsf(disagreement);
  s->output.innovation_rad = innovation;
  const float pair_quality = 1.0f - fabsf(disagreement) / c->max_pair_disagreement_rad;
  const float innovation_quality = 1.0f - fabsf(innovation) / c->max_innovation_rad;
  s->output.quality = acquired ? fmaxf(0.0f, fminf(pair_quality, innovation_quality)) : 0.0f;
  return acquired;
}
