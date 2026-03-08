#ifndef TSM_CAN_H
#define TSM_CAN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void tsm_can_init(void);

/* Update the sensor CAN packet */
void tsm_can_update_sensors(uint16_t therm1, uint16_t therm2, uint16_t therm3,
                            uint16_t therm4, float coolant_flow_lpm,
                            float fan_rpm);

typedef struct {
  uint16_t thermistor1;
  uint16_t thermistor2;
  uint16_t thermistor3;
  uint16_t thermistor4;
  float coolant_flow_lpm;
  float fan_rpm;
} msg_tsm_sensors_t;

#ifdef __cplusplus
}
#endif

#endif