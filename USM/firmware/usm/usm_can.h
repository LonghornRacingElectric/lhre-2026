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
 * The wheel speed is reported in the unified "Wheel Speeds" CAN message (ID 1024)
 * which includes speeds for all 4 corners. This board (based on its location)
 * updates its corner while zeroing the others.
 *
 * @param wheel_speed_rads  wheel speed in rad/s
 */
void usm_can_update_wheel_speed(float wheel_speed_rads);

/**
 * Update the unsprung acceleration for this corner.
 * Call this from the IMU read task with the latest acceleration values.
 * 
 * Each corner sends acceleration via its own dedicated CAN message:
 * - FL uses message ID 1026
 * - FR uses message ID 1027
 * - RL uses message ID 1028
 * - RR uses message ID 1029
 *
 * @param ax  acceleration in X axis (m/s^2)
 * @param ay  acceleration in Y axis (m/s^2)
 * @param az  acceleration in Z axis (m/s^2)
 */
void usm_can_update_accel(float ax, float ay, float az);

/**
 * Print debug information about CAN bus status.
 */
void usm_can_debug(void);

#ifdef __cplusplus
}
#endif

#endif
