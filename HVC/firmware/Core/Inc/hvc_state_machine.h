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
#include <stdbool.h>
#include <stdint.h>

/* Exported types ------------------------------------------------------------*/

/**
 * @brief HVC State Machine States
 */
typedef enum {
    HVC_STATE_NOT_ENERGIZED = 0,        /**< System powered down, contactors open */
    HVC_STATE_PRECHARGING,              /**< Precharge in progress */
    HVC_STATE_ENERGIZED,                /**< System powered, ready to drive */
    HVC_STATE_CHARGING_PRECHARGING,     /**< Charging mode precharge */
    HVC_STATE_CHARGING                  /**< Charging mode active */
} hvc_state_t;

/* Exported constants --------------------------------------------------------*/

#define HVC_PRECHARGE_THRESHOLD_PERCENT     83      /**< Precharge voltage threshold (% of pack) */
#define HVC_PRECHARGE_VALID_MS            5000    /**< Precharge timeout (milliseconds) */
#define HVC_FAULT_HYSTERESIS_MS             5000    /**< Fault detection hysteresis time */

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
void update_state_machine(void);

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
const char* get_state_name(hvc_state_t state);

/* Sensor/Flag Interface Functions (to be implemented by other modules) ------*/

/**
 * @brief Check if tractive system enable (TS_Enable) is active
 * @return true if TS_Enable is active, false otherwise
 */
bool is_shutdown_closed(void);

/**
 * @brief Check if any faults are present
 * @return true if fault detected, false otherwise
 */
bool is_fault_present(void);

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
