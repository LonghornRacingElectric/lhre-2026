#include <stdint.h>

void flow_fan_update(uint32_t *last_flow, uint32_t *last_rad_fan,
                     uint32_t *last_bat_fan, float *coolant_lpm,
                     float *rad_fan_rpm, float *bat_fan_rpm,
                     uint32_t *last_tick);