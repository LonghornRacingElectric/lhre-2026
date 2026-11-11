#ifndef USM_WHEEL_PHASE_H
#define USM_WHEEL_PHASE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WHEEL_PHASE_CHANNELS 4u
#define WHEEL_PHASE_ALL_CHANNELS 0x0fu

/* Fixed Orion geometry: four sensors at 0/15/30/45 mechanical degrees,
 * three magnetic pole pairs. Inputs are each sensor's LOCAL radial field.
 * After offset/scale/sign calibration, the expected signals are
 * cos(phi), cos(phi-pi/4), sin(phi), sin(phi-pi/4).
 * Signs establish the positive wheel direction; do not infer it from speed.
 * This model requires bench validation: discrete magnets need not be sinusoidal.
 */
typedef struct {
  bool calibrated; /* Defaults false. Never enable from guessed coefficients. */
  float offset[WHEEL_PHASE_CHANNELS];
  float scale[WHEEL_PHASE_CHANNELS]; /* Positive field units per unit amplitude. */
  int8_t sign[WHEEL_PHASE_CHANNELS]; /* Exactly +1 or -1. */
  float pair1_correction_rad; /* Added to atan2(channel 3, channel 1). */
  float min_magnitude;        /* Normalized pair radius acceptance window. */
  float max_magnitude;
  float max_pair_disagreement_rad;
  float phase_noise_bound_rad; /* Measured engineering bound, not covariance. */
  float max_speed_rad_s;       /* Mechanical speed; sets unwrap alias bound. */
  float max_innovation_rad;
  float alpha;
  float beta;
  uint32_t min_dt_us;
  uint32_t max_gap_us;
  uint32_t max_sample_age_us;
  uint32_t max_channel_skew_us;
  uint32_t acquire_time_us;
  uint16_t acquire_samples;
} wheel_phase_config_t;

typedef struct {
  float radial[WHEEL_PHASE_CHANNELS];
  /* Acquisition timestamps on ONE monotonic USM clock, not SPI read time.
   * All differences must be < 2^31 us. uint32 wrap is supported. */
  uint32_t sample_time_us[WHEEL_PHASE_CHANNELS];
  uint8_t valid_mask; /* Driver has checked transfer, sensor status and CRC. */
  uint8_t fresh_mask; /* New measurement, not merely another SPI read. */
} wheel_phase_sample_t;

enum {
  WHEEL_PHASE_UNCALIBRATED = 1u << 0,
  WHEEL_PHASE_BAD_CONFIG = 1u << 1,
  WHEEL_PHASE_BAD_SAMPLE = 1u << 2,
  WHEEL_PHASE_STALE = 1u << 3,
  WHEEL_PHASE_BAD_TIME = 1u << 4,
  WHEEL_PHASE_SKEW = 1u << 5,
  WHEEL_PHASE_BAD_MAGNITUDE = 1u << 6,
  WHEEL_PHASE_PAIR_DISAGREEMENT = 1u << 7,
  WHEEL_PHASE_ALIAS = 1u << 8,
  WHEEL_PHASE_GAP = 1u << 9,
  WHEEL_PHASE_INNOVATION = 1u << 10,
  WHEEL_PHASE_ACQUIRING = 1u << 11
};

typedef struct {
  bool valid;       /* Speed usable only when true. */
  bool phase_valid;
  uint32_t status;
  uint32_t sample_time_us; /* Latest channel acquisition time. */
  float speed_rad_s;       /* Signed MECHANICAL wheel speed. */
  float electrical_phase_rad; /* Wrapped [-pi, pi), not absolute wheel angle. */
  double relative_angle_rad;  /* Mechanical angle since last acquisition seed. */
  float pair_disagreement_rad;
  float innovation_rad;
  float quality; /* [0,1] diagnostic score, NOT a probability/accuracy guarantee. */
} wheel_phase_output_t;

typedef struct {
  wheel_phase_config_t config;
  wheel_phase_output_t output;
  bool configured;
  bool tracking;
  bool have_channel_times;
  uint32_t last_channel_time_us[WHEEL_PHASE_CHANNELS];
  uint32_t last_time_us;
  uint32_t acquisition_start_us;
  uint16_t accepted_samples;
  float last_measured_phase_rad;
  float estimated_phase_rad;
  float electrical_speed_rad_s;
  double relative_electrical_angle_rad;
} wheel_phase_t;

/* Defaults are commissioning starting points, not validated race calibration. */
wheel_phase_config_t wheel_phase_default_config(void);
bool wheel_phase_init(wheel_phase_t *state, const wheel_phase_config_t *config);
/* Clears tracking and history while retaining configuration. */
void wheel_phase_reset(wheel_phase_t *state);
/* Returns true only for an acquired, valid speed. Invalid input drops validity
 * immediately and requires reacquisition; no old speed is returned as valid.
 * Read state->output for the reason and intermediate phase diagnostics.
 * Call once per fresh complete set; this function is not a freshness watchdog.
 * Consumers must separately time out output.sample_time_us if updates stop. */
bool wheel_phase_update(wheel_phase_t *state,
                        const wheel_phase_sample_t *sample, uint32_t now_us);

#ifdef __cplusplus
}
#endif
#endif
