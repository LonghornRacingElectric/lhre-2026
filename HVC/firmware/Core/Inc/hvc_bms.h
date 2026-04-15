/**
 * @file hvc_bms.h
 * @brief High Voltage Controller - Battery Management System Interface
 * 
 * Clean implementation built from ADBMS6830 datasheet understanding.
 * To be developed iteratively with proper protocol comprehension.
 */

#ifndef HVC_BMS_H
#define HVC_BMS_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize the BMS subsystem
 * 
 * Sets up SPI communication and prepares for ADBMS6830 interaction.
 * To be implemented based on datasheet specifications.
 */
void bms_init(void);

/**
 * @brief Update BMS readings and state
 * 
 * Called periodically (5Hz recommended) to communicate with BMS chips.
 * To be implemented based on datasheet command protocol.
 */
void bms_update(void);

/**
 * @brief Read thermistor values from GPIO 2-9 on each BMB
 * 
 * Reads auxiliary ADC channels corresponding to GPIO 2-9 which are
 * connected to analog thermistor inputs for temperature sensing.
 */
void bms_read_thermistors(void);

uint32_t bms_get_num_responsive_ics(void);
bool bms_check_disconnection(void);
bool bms_check_undervoltage(void);
bool bms_check_overvoltage(void);
bool bms_check_overtemp(void);

float getPackVoltage_v(void);
float bms_get_min_voltage(void);
float bms_get_max_voltage(void);
float bms_get_min_temp(void);
float bms_get_max_temp(void);

void StartBmsTask(void *argument);

#endif // HVC_BMS_H
