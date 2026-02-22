#include "util.h"

float clamp_f(float x, float lo, float hi) {
  if (x < lo)
    return lo;
  if (x > hi)
    return hi;
  return x;
}

uint16_t clamp_u16(uint16_t x, uint16_t lo, uint16_t hi) {
  if (x < lo)
    return lo;
  if (x > hi)
    return hi;
  return x;
}

uint32_t clamp_u32(uint32_t x, uint32_t lo, uint32_t hi) {
  if (x < lo)
    return lo;
  if (x > hi)
    return hi;
  return x;
}

float linear_interp(float a, float b, float pct) { return a + pct * (b - a); }
float inverse_linear_interp(float a, float b, float val) {
  float span = b - a;
  if (span <= 1e-6)
    return 0.0f;
  return clamp_f((val - a) / span, 0.0f, 1.0f);
}

float apply_deadzone(float value, float min_threshold, float max_threshold) {
  if (value <= min_threshold) {
    return 0.0f;
  }
  if (value >= max_threshold) {
    return 1.0f;
  }
  return inverse_linear_interp(min_threshold, max_threshold, value);
}

bool rising_edge(bool prev, bool curr) { return curr && !prev; }
