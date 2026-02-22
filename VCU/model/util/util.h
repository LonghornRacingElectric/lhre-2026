#ifndef VCU_UTIL_H
#define VCU_UTIL_H

#include <stdbool.h>
#include <stdint.h>

float clamp_f(float x, float lo, float hi);
uint16_t clamp_u16(uint16_t x, uint16_t lo, uint16_t hi);
uint32_t clamp_u32(uint32_t x, uint32_t lo, uint32_t hi);

float linear_interp(float a, float b, float pct);
float inverse_linear_interp(float a, float b, float val);

bool rising_edge(bool prev, bool curr);

#endif // VCU_UTIL_H
