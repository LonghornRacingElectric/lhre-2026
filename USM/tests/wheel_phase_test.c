#include "wheel_phase.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define PI 3.14159265358979323846f
#define CHECK(condition) do { \
  if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    exit(1); \
  } \
} while (0)
#define NEAR(actual, expected, tolerance) \
  CHECK(fabs((double)(actual) - (double)(expected)) <= (double)(tolerance))

static wheel_phase_sample_t sample_at(float phase, uint32_t time) {
  wheel_phase_sample_t sample = {0};
  sample.valid_mask = WHEEL_PHASE_ALL_CHANNELS;
  sample.fresh_mask = WHEEL_PHASE_ALL_CHANNELS;
  for (unsigned i = 0; i < WHEEL_PHASE_CHANNELS; ++i) {
    sample.radial[i] = cosf(phase - (float)i * PI / 4.0f);
    sample.sample_time_us[i] = time;
  }
  return sample;
}

static wheel_phase_config_t calibrated_config(void) {
  wheel_phase_config_t c = wheel_phase_default_config();
  /* Synthetic ideal signals have known calibration. Production defaults do not. */
  c.calibrated = true;
  return c;
}

static void acquire_stationary(wheel_phase_t *s, uint32_t start) {
  for (unsigned i = 0; i <= 30; ++i) {
    uint32_t time = start + i * 500u;
    wheel_phase_sample_t sample = sample_at(0.7f, time);
    wheel_phase_update(s, &sample, time);
  }
  CHECK(s->output.valid);
  NEAR(s->output.speed_rad_s, 0.0, 0.001);
}

static void test_default_disabled_and_config(void) {
  wheel_phase_t s;
  wheel_phase_config_t c = wheel_phase_default_config();
  CHECK(!c.calibrated);
  CHECK(wheel_phase_init(&s, &c));
  wheel_phase_sample_t sample = sample_at(0.4f, 1000);
  CHECK(!wheel_phase_update(&s, &sample, 1000));
  CHECK(s.output.status == WHEEL_PHASE_UNCALIBRATED);
  CHECK(!s.output.phase_valid);
  CHECK(!wheel_phase_init(NULL, &c));
  CHECK(!wheel_phase_init(&s, NULL));
  c = calibrated_config();
  c.scale[1] = 0;
  CHECK(!wheel_phase_init(&s, &c));
  CHECK(s.output.status == WHEEL_PHASE_BAD_CONFIG);
  c = calibrated_config(); c.sign[2] = 0;
  CHECK(!wheel_phase_init(&s, &c));
  c = calibrated_config(); c.offset[3] = NAN;
  CHECK(!wheel_phase_init(&s, &c));
  c = calibrated_config(); c.alpha = NAN;
  CHECK(!wheel_phase_init(&s, &c));
  c = calibrated_config(); c.beta = 4.0f;
  CHECK(!wheel_phase_init(&s, &c));
  c = calibrated_config(); c.acquire_samples = 1;
  CHECK(!wheel_phase_init(&s, &c));
  c = calibrated_config(); c.max_sample_age_us = UINT32_MAX;
  CHECK(!wheel_phase_init(&s, &c));
  c = calibrated_config(); c.pair1_correction_rad = INFINITY;
  CHECK(!wheel_phase_init(&s, &c));
  c = calibrated_config(); CHECK(wheel_phase_init(&s, &c));
  CHECK(wheel_phase_init(&s, &s.config));
}

static void test_stationary_and_calibration(void) {
  wheel_phase_t s;
  wheel_phase_config_t c = calibrated_config();
  for (unsigned i = 0; i < WHEEL_PHASE_CHANNELS; ++i) {
    c.offset[i] = 0.7f * (float)i - 0.9f;
    c.scale[i] = 2.0f + (float)i;
    c.sign[i] = (i % 2 == 0) ? 1 : -1;
  }
  CHECK(wheel_phase_init(&s, &c));
  for (unsigned i = 0; i < 80; ++i) {
    uint32_t time = 70000 + i * 500u;
    wheel_phase_sample_t sample = sample_at(2.8f, time);
    for (unsigned j = 0; j < WHEEL_PHASE_CHANNELS; ++j)
      sample.radial[j] = c.offset[j] + sample.radial[j] * c.scale[j] * (float)c.sign[j];
    bool valid = wheel_phase_update(&s, &sample, time);
    CHECK(valid == (i >= 20));
  }
  NEAR(s.output.speed_rad_s, 0, 0.001);
  NEAR(s.output.electrical_phase_rad, 2.8, 0.00001);
  NEAR(s.output.relative_angle_rad, 0, 0.00001);
  CHECK(s.output.quality > 0.999f);
}

static void test_speed_direction_wrap_and_variable_dt(void) {
  const float speeds[] = {0.05f, 15.0f, -15.0f, 220.0f, -220.0f};
  for (unsigned test = 0; test < sizeof(speeds) / sizeof(speeds[0]); ++test) {
    wheel_phase_t s;
    wheel_phase_config_t c = calibrated_config();
    CHECK(wheel_phase_init(&s, &c));
    uint32_t time = UINT32_MAX - 3000u;
    double elapsed = 0;
    float phase = 3.10f;
    wheel_phase_sample_t sample = sample_at(phase, time);
    CHECK(!wheel_phase_update(&s, &sample, time));
    for (unsigned i = 0; i < 800; ++i) {
      uint32_t step_us = 400u + (i % 3u) * 100u;
      time += step_us;
      elapsed += (double)step_us * 1e-6;
      phase = (float)remainder(3.10 + (double)speeds[test] * 3.0 * elapsed,
                                2.0 * (double)PI);
      sample = sample_at(phase, time);
      wheel_phase_update(&s, &sample, time);
      if (i > 30) CHECK(s.output.valid);
    }
    NEAR(s.output.speed_rad_s, speeds[test], 0.008);
    NEAR(s.output.relative_angle_rad, speeds[test] * elapsed, 0.0001);
    CHECK(s.output.electrical_phase_rad >= -PI && s.output.electrical_phase_rad < PI);
  }
}

static void test_launch_and_reversal(void) {
  wheel_phase_t s;
  wheel_phase_config_t c = calibrated_config();
  CHECK(wheel_phase_init(&s, &c));
  acquire_stationary(&s, 100000);
  double phase = 0.7;
  float speed = 0;
  uint32_t time = 115000;
  for (unsigned i = 0; i < 1000; ++i) {
    float acceleration = i < 500 ? 50.0f : -100.0f;
    phase += 3.0 * ((double)speed * 0.0005 + 0.5 * acceleration * 0.0005 * 0.0005);
    speed += acceleration * 0.0005f;
    time += 500;
    wheel_phase_sample_t sample = sample_at((float)remainder(phase, 2.0 * PI), time);
    CHECK(wheel_phase_update(&s, &sample, time));
    NEAR(s.output.speed_rad_s, speed, 0.5);
  }
  CHECK(s.output.speed_rad_s < 0);
}

static void test_faults_drop_validity_and_reacquire(void) {
  for (unsigned test = 0; test < 10; ++test) {
    wheel_phase_t s;
    wheel_phase_config_t c = calibrated_config();
    CHECK(wheel_phase_init(&s, &c));
    acquire_stationary(&s, 100000);
    wheel_phase_sample_t sample = sample_at(0.7f, 115500);
    uint32_t now = 115500;
    uint32_t expected = 0;
    switch (test) {
      case 0: sample.radial[0] = NAN; expected = WHEEL_PHASE_BAD_SAMPLE; break;
      case 1: sample.radial[0] = INFINITY; expected = WHEEL_PHASE_BAD_SAMPLE; break;
      case 2: sample.radial[0] = 0; sample.radial[2] = 0;
              expected = WHEEL_PHASE_BAD_MAGNITUDE; break;
      case 3: sample.radial[1] = cosf(0.7f - PI/4 + 0.5f);
              sample.radial[3] = sinf(0.7f - PI/4 + 0.5f);
              expected = WHEEL_PHASE_PAIR_DISAGREEMENT; break;
      case 4: sample.valid_mask = 7; expected = WHEEL_PHASE_BAD_SAMPLE; break;
      case 5: sample.fresh_mask = 7; expected = WHEEL_PHASE_STALE; break;
      case 6: sample.sample_time_us[0] -= 200; expected = WHEEL_PHASE_SKEW; break;
      case 7: now += 3000; expected = WHEEL_PHASE_STALE; break;
      case 8: sample.sample_time_us[0] += 1; expected = WHEEL_PHASE_BAD_TIME; break;
      case 9: sample.radial[1] *= 10; expected = WHEEL_PHASE_BAD_MAGNITUDE; break;
    }
    CHECK(!wheel_phase_update(&s, &sample, now));
    CHECK(s.output.status == expected);
    CHECK(!s.output.valid && !s.output.phase_valid);
    NEAR(s.output.speed_rad_s, 0, 0);
    sample = sample_at(0.7f, 120000);
    CHECK(!wheel_phase_update(&s, &sample, 120000));
    CHECK(s.output.status == WHEEL_PHASE_ACQUIRING);
    CHECK(s.output.phase_valid);
    acquire_stationary(&s, 120500);
  }
}

static void test_duplicate_backward_gap_and_alias(void) {
  wheel_phase_t s;
  wheel_phase_config_t c = calibrated_config();
  CHECK(wheel_phase_init(&s, &c));
  acquire_stationary(&s, 100000);
  wheel_phase_sample_t sample = sample_at(0.7f, 115000);
  CHECK(!wheel_phase_update(&s, &sample, 115100));
  CHECK(s.output.status == WHEEL_PHASE_STALE);
  sample = sample_at(0.7f, 114999);
  CHECK(!wheel_phase_update(&s, &sample, 115100));
  CHECK(s.output.status == WHEEL_PHASE_BAD_TIME);
  acquire_stationary(&s, 120000);
  sample = sample_at(0.7f, 140000);
  CHECK(!wheel_phase_update(&s, &sample, 140000));
  CHECK(s.output.status == WHEEL_PHASE_GAP);
  acquire_stationary(&s, 141000);
  sample = sample_at(2.0f, 156500);
  CHECK(!wheel_phase_update(&s, &sample, 156500));
  CHECK(s.output.status == WHEEL_PHASE_ALIAS);
  c.max_gap_us = 10000;
  CHECK(wheel_phase_init(&s, &c));
  acquire_stationary(&s, 200000);
  sample = sample_at(0.7f, 220000);
  CHECK(!wheel_phase_update(&s, &sample, 220000));
  CHECK(s.output.status == WHEEL_PHASE_ALIAS);
  wheel_phase_reset(&s);
  CHECK(!s.output.valid && s.output.status == WHEEL_PHASE_ACQUIRING);
  sample = sample_at(0.7f, 1);
  CHECK(!wheel_phase_update(&s, &sample, 1));
  CHECK(s.output.phase_valid);
}

static void test_skew_compensation_and_bounded_noise(void) {
  wheel_phase_t s;
  wheel_phase_config_t c = calibrated_config();
  CHECK(wheel_phase_init(&s, &c));
  const float speed = 80.0f;
  for (unsigned i = 0; i < 600; ++i) {
    uint32_t now = 100000u + i * 500u;
    wheel_phase_sample_t sample = sample_at(0, now);
    for (unsigned j = 0; j < WHEEL_PHASE_CHANNELS; ++j) {
      uint32_t age = j * 20u;
      sample.sample_time_us[j] = now - age;
      double elapsed = ((double)i * 500.0 - (double)age) * 1e-6;
      sample.radial[j] = cosf((float)remainder(0.6 + 3.0 * speed * elapsed,
                                               2.0 * PI) - (float)j * PI / 4.0f);
      sample.radial[j] += 0.001f * sinf((float)(i * 7u + j * 11u));
    }
    wheel_phase_update(&s, &sample, now);
    if (i > 30) {
      CHECK(s.output.valid);
      NEAR(s.output.speed_rad_s, speed, 0.8);
      CHECK(s.output.quality >= 0 && s.output.quality <= 1);
    }
  }
}

static void test_innovation_short_interval_and_null(void) {
  wheel_phase_t s;
  wheel_phase_config_t c = calibrated_config();
  c.max_innovation_rad = 0.05f;
  CHECK(wheel_phase_init(&s, &c));
  acquire_stationary(&s, 100000);
  wheel_phase_sample_t sample = sample_at(0.8f, 115500);
  CHECK(!wheel_phase_update(&s, &sample, 115500));
  CHECK(s.output.status == WHEEL_PHASE_INNOVATION);
  acquire_stationary(&s, 120000);
  sample = sample_at(0.7f, 135001);
  CHECK(!wheel_phase_update(&s, &sample, 135001));
  CHECK(s.output.status == WHEEL_PHASE_BAD_TIME);
  CHECK(!wheel_phase_update(&s, NULL, 135500));
  CHECK(s.output.status == WHEEL_PHASE_BAD_SAMPLE);
  CHECK(!wheel_phase_update(NULL, &sample, 135500));
}

static void test_stationary_with_noise(void) {
  wheel_phase_t s;
  wheel_phase_config_t c = calibrated_config();
  CHECK(wheel_phase_init(&s, &c));
  for (unsigned i = 0; i < 1000; ++i) {
    uint32_t now = 500000 + i * 500u;
    wheel_phase_sample_t sample = sample_at(-2.9f, now);
    for (unsigned j = 0; j < WHEEL_PHASE_CHANNELS; ++j)
      sample.radial[j] += 0.002f * sinf((float)i * 0.7f + (float)j);
    wheel_phase_update(&s, &sample, now);
    if (i > 30) {
      CHECK(s.output.valid);
      NEAR(s.output.speed_rad_s, 0, 0.5);
    }
  }
}

int main(void) {
  test_default_disabled_and_config();
  test_stationary_and_calibration();
  test_speed_direction_wrap_and_variable_dt();
  test_launch_and_reversal();
  test_faults_drop_validity_and_reacquire();
  test_duplicate_backward_gap_and_alias();
  test_skew_compensation_and_bounded_noise();
  test_innovation_short_interval_and_null();
  test_stationary_with_noise();
  puts("wheel_phase_test: all cases passed");
  return 0;
}
