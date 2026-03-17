#include <stdint.h>

void flow_fan_update(uint32_t *last_flow, uint32_t *last_fan,
                     float *coolant_lpm, float *fan_rpm);