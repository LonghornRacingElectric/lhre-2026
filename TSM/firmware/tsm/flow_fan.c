#include "flow_fan.h"
#include "stm32g4xx_hal.h"

extern volatile uint32_t flow_pulses;
extern volatile uint32_t fan_pulses;

void flow_fan_update(uint32_t *last_flow, uint32_t *last_fan,
                     float *coolant_lpm, float *fan_rpm, uint32_t *last_tick) {

  uint32_t now = HAL_GetTick();
  uint32_t elapsed_ms = now - *last_tick;
  *last_tick = now;

  if (elapsed_ms == 0)
    return;

  uint32_t flow_now = flow_pulses;
  uint32_t flow_delta = flow_now - *last_flow;
  *last_flow = flow_now;
  *coolant_lpm = (flow_delta * 60000.0f) / (169.0f * elapsed_ms);

  uint32_t fan_now = fan_pulses;
  uint32_t fan_delta = fan_now - *last_fan;
  *last_fan = fan_now;
  *fan_rpm = (fan_delta * 30000.0f) / (elapsed_ms * 1.0f); // 2 pulses/rev
}