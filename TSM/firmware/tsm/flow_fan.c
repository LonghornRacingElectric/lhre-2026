#include "flow_fan.h"

extern volatile uint32_t flow_pulses;
extern volatile uint32_t fan_pulses;

void flow_fan_update(uint32_t *last_flow, uint32_t *last_fan,
                     float *coolant_lpm, float *fan_rpm) {
  uint32_t flow_now = flow_pulses;
  uint32_t flow_delta = flow_now - *last_flow;
  *last_flow = flow_now;
  *coolant_lpm = (flow_delta * 60.0f) / 169.0f;

  uint32_t fan_now = fan_pulses;
  uint32_t fan_delta = fan_now - *last_fan;
  *last_fan = fan_now;
  *fan_rpm = (fan_delta * 60.0f) / 2.0f;
}