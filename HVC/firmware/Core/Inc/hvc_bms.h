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

/**
 * @brief Check all cells for safety violations
 * 
 * Checks all cell voltages and temperatures against thresholds.
 * Sets error flags if overvoltage, undervoltage, or overtemperature detected.
 */
void bms_check_errors(void);

/**
 * @brief Get current BMS error flags
 * 
 * @return Bitmask of active errors (BMS_ERROR_OVERVOLTAGE, BMS_ERROR_UNDERVOLTAGE, BMS_ERROR_OVERTEMP)
 */
uint8_t getBmsErrors(void);

float getPackVoltage_v(void);

void StartBmsTask(void *argument);

#endif // HVC_BMS_H
