#ifndef TSM_CAN_H
#define TSM_CAN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the TSM CAN interface and start RTOS CAN tasks
 */
void tsm_can_init(void);

/**
 * Update coolant loop temperatures
 *
 * @param loop_motor_temp        temp after motor (°C)
 * @param loop_inverter_temp     temp after inverter (°C)
 * @param radiator_outlet_temp   temp after radiator (°C)
 */
void tsm_can_update_coolant_loop(float loop_motor_temp,
                                 float loop_inverter_temp,
                                 float radiator_outlet_temp);

/**
 * Update cooling system telemetry
 *
 * @param fan_rpm           radiator fan speed (rpm)
 * @param coolant_flow_lpm  coolant flow rate (L/min)
 * @param ambient_temp      ambient temperature (°C)
 */
void tsm_can_update_cooling_system(float fan_rpm, float coolant_flow_lpm,
                                   float ambient_temp);

#ifdef __cplusplus
}
#endif

#endif