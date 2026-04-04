#ifndef USM_CAN_H
#define USM_CAN_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the USM CAN interface and start RTOS CAN tasks.
 */
void usm_can_init(void);

/**
 * Update the wheel speed value for this corner.
 * Call this from the wheel speed task whenever a new reading is available.
 *
 * @param wheel_speed_rads  wheel speed in rad/s
 */
void usm_can_update_wheel_speed(float wheel_speed_rads);

/**
 * Update the unsprung acceleration for this corner.
 * @param ax/ay/az  acceleration in m/s^2
 */
void usm_can_update_accel(float ax, float ay, float az);

#ifdef __cplusplus
}
#endif

#endif
