/**
 ******************************************************************************
 * @file    hvc_state_machine.h
 * @brief   HVC State Machine Module Header
 * @details State machine for High Voltage Controller managing precharge,
 *          contactor control, and charging states. Ported from 2024 bare-metal
 *          implementation to FreeRTOS architecture.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 Longhorn Racing Electric
 * All rights reserved.
 *
 ******************************************************************************
 */

#ifndef HVC_STATE_MACHINE_H
#define HVC_STATE_MACHINE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "hvc_states.h"
#include <stdbool.h>
#include <stdint.h>

/* Exported constants --------------------------------------------------------*/

#define HVC_PRECHARGE_THRESHOLD_PERCENT 0.90f
#define HVC_PRECHARGE_VALID_MS 5000

/* Exported functions --------------------------------------------------------*/

/**
 * @brief Initialize the state machine
 * @note Must be called before update_state_machine()
 */
void state_machine_init(void);

/**
 * @brief Update state machine (called periodically by RTOS task)
 * @note Should be called at 10Hz
 */
void update_state_machine(bool any_faults);

/**
 * @brief Get current state
 * @return Current state machine state
 */
hvc_state_t get_current_state(void);

/**
 * @brief Get state name as string (for logging)
 * @param state State to convert to string
 * @return String representation of state
 */
const char *get_state_name(hvc_state_t state);

/* Sensor/Flag Interface Functions (to be implemented by other modules) ------*/

/**
 * @brief Check if tractive system enable (TS_Enable) is active
 * @return true if TS_Enable is active, false otherwise
 */
bool is_shutdown_closed(void);

/**
 * @brief Check if charge enable is active
 * @return true if charge enable active, false otherwise
 */
bool is_charge_enable_active(void);

/**
 * @brief Get tractive system voltage (V)
 * @return Voltage in volts
 */
float get_tractive_voltage(void);

/**
 * @brief Get battery pack voltage (V)
 * @return Voltage in volts
 */
float get_pack_voltage(void);

#ifdef __cplusplus
}
#endif

#endif /* HVC_STATE_MACHINE_H */
