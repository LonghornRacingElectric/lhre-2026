#include "flow_fan.h"
#include "stm32g4xx_hal.h"

extern volatile uint32_t flow_pulses;
extern volatile uint32_t fan_pulses;
extern volatile uint32_t bat_fan_pulses;

#define FLOW_PULSES_PER_LITER 195.0f  // 10mm nozzle: LPM = 0.307 * Hz -> 60/0.307 pulses/L

#define RAD_FAN_PULSES_PER_REV 4.0f  // CFM-A225/A238 radiator fans
#define BAT_FAN_PULSES_PER_REV 2.0f  // San Ace 9HV3612P3K001 battery fan

void flow_fan_update(uint32_t *last_flow, uint32_t *last_rad_fan,
                     uint32_t *last_bat_fan, float *coolant_lpm,
                     float *rad_fan_rpm, float *bat_fan_rpm,
                     uint32_t *last_tick) {

  uint32_t now = HAL_GetTick();
  uint32_t elapsed_ms = now - *last_tick;
  *last_tick = now;

  if (elapsed_ms == 0)
    return;

  uint32_t flow_now = flow_pulses;
  uint32_t flow_delta = flow_now - *last_flow;
  *last_flow = flow_now;
  // pulses / (pulses/L) / (ms) * (60000 ms/min) = L/min
  *coolant_lpm = (flow_delta * 60000.0f) / (FLOW_PULSES_PER_LITER * elapsed_ms);

  uint32_t rad_fan_now = fan_pulses;
  uint32_t rad_fan_delta = rad_fan_now - *last_rad_fan;
  *last_rad_fan = rad_fan_now;
  // pulses / (pulses/rev) / (ms) * (60000 ms/min) = RPM
  *rad_fan_rpm = (rad_fan_delta * 60000.0f) / (RAD_FAN_PULSES_PER_REV * elapsed_ms);

  uint32_t bat_fan_now = bat_fan_pulses;
  uint32_t bat_fan_delta = bat_fan_now - *last_bat_fan;
  *last_bat_fan = bat_fan_now;
  *bat_fan_rpm = (bat_fan_delta * 60000.0f) / (BAT_FAN_PULSES_PER_REV * elapsed_ms);
}