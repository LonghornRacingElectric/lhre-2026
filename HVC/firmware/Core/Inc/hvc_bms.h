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

void StartBmsTask(void *argument);

#endif // HVC_BMS_H
