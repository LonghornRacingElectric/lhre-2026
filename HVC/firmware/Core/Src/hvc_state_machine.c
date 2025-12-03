/**
 ******************************************************************************
 * @file    hvc_state_machine.c
 * @brief   HVC State Machine Implementation
 * @details State machine logic ported from 2024 bare-metal implementation.
 *          Manages precharge sequence, contactor control, and charging states.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 Longhorn Racing Electric
 * All rights reserved.
 *
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "hvc_state_machine.h"
#include "hvc_contactors.h"
#include "cmsis_os.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

static hvc_state_t current_state = HVC_STATE_NOT_ENERGIZED;
static uint32_t precharge_start_time = 0;

/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/

/**
 * @brief Initialize the state machine
 */
void state_machine_init(void) {
    current_state = HVC_STATE_NOT_ENERGIZED;
    precharge_start_time = 0;
    
    // Ensure all contactors are open on startup
    open_all_contactors();
}

/**
 * @brief Update state machine
 * @note Based on 2024 implementation with ~80 lines of proven logic
 */
void update_state_machine(void) {
    uint32_t current_time = osKernelGetTickCount();
    
    // Check for faults - immediately return to NOT_ENERGIZED if fault detected
    if (is_fault_present()) {
        if (current_state != HVC_STATE_NOT_ENERGIZED) {
            open_all_contactors();
            current_state = HVC_STATE_NOT_ENERGIZED;
        }
        return;
    }
    
    // State machine logic
    switch (current_state) {
        case HVC_STATE_NOT_ENERGIZED:
            // Stay open until TS_Enable is active
            open_all_contactors();
            
            if (is_ts_enable_active() && !is_charge_enable_active()) {
                // Transition to precharging
                set_precharge_contactor(true);
                precharge_start_time = current_time;
                current_state = HVC_STATE_PRECHARGING;
            } else if (is_charge_enable_active()) {
                // Transition to charging precharge
                set_precharge_contactor(true);
                precharge_start_time = current_time;
                current_state = HVC_STATE_CHARGING_PRECHARGING;
            }
            break;
            
        case HVC_STATE_PRECHARGING:
            // Check if precharge complete
            float tractive_voltage = get_tractive_voltage();
            float pack_voltage = get_pack_voltage();
            float precharge_threshold = pack_voltage * HVC_PRECHARGE_THRESHOLD_PERCENT / 100.0f;
            
            if (tractive_voltage > precharge_threshold) {
                uint32_t elapsed = current_time - precharge_start_time;
                
                if (elapsed >= HVC_PRECHARGE_TIMEOUT_MS) {
                    // Precharge complete - close drive contactors
                    set_drive_contactors(true);
                    set_precharge_contactor(false);
                    current_state = HVC_STATE_ENERGIZED;
                }
            } else {
                // Reset timer if voltage drops
                precharge_start_time = current_time;
            }
            
            // Check if TS_Enable released
            if (!is_ts_enable_active()) {
                open_all_contactors();
                current_state = HVC_STATE_NOT_ENERGIZED;
            }
            break;
            
        case HVC_STATE_ENERGIZED:
            // Stay energized while TS_Enable is active
            if (!is_ts_enable_active()) {
                open_all_contactors();
                current_state = HVC_STATE_NOT_ENERGIZED;
            }
            break;
            
        case HVC_STATE_CHARGING_PRECHARGING:
            // Similar to normal precharge, but for charging
            tractive_voltage = get_tractive_voltage();
            pack_voltage = get_pack_voltage();
            precharge_threshold = pack_voltage * HVC_PRECHARGE_THRESHOLD_PERCENT / 100.0f;
            
            if (tractive_voltage > precharge_threshold) {
                uint32_t elapsed = current_time - precharge_start_time;
                
                if (elapsed >= HVC_PRECHARGE_TIMEOUT_MS) {
                    // Precharge complete - transition to charging
                    set_drive_contactors(true);
                    set_precharge_contactor(false);
                    current_state = HVC_STATE_CHARGING;
                }
            } else {
                // Reset timer if voltage drops
                precharge_start_time = current_time;
            }
            
            // Check if charge enable released
            if (!is_charge_enable_active()) {
                open_all_contactors();
                current_state = HVC_STATE_NOT_ENERGIZED;
            }
            break;
            
        case HVC_STATE_CHARGING:
            // Stay in charging while charge enable is active
            if (!is_charge_enable_active()) {
                open_all_contactors();
                current_state = HVC_STATE_NOT_ENERGIZED;
            }
            break;
            
        default:
            // Invalid state - go to safe state
            open_all_contactors();
            current_state = HVC_STATE_NOT_ENERGIZED;
            break;
    }
}

/**
 * @brief Get current state
 */
hvc_state_t get_current_state(void) {
    return current_state;
}

/**
 * @brief Get state name as string
 */
const char* get_state_name(hvc_state_t state) {
    switch (state) {
        case HVC_STATE_NOT_ENERGIZED:
            return "NOT_ENERGIZED";
        case HVC_STATE_PRECHARGING:
            return "PRECHARGING";
        case HVC_STATE_ENERGIZED:
            return "ENERGIZED";
        case HVC_STATE_CHARGING_PRECHARGING:
            return "CHARGING_PRECHARGING";
        case HVC_STATE_CHARGING:
            return "CHARGING";
        default:
            return "UNKNOWN";
    }
}

/* Weak implementations of sensor interface functions ------------------------*/
/* These provide safe defaults until real implementations are added -----------*/

__attribute__((weak)) bool is_ts_enable_active(void) {
    // Default: return false (safe default - no enable)
    return false;
}

__attribute__((weak)) bool is_fault_present(void) {
    // Default: return false (no fault)
    return false;
}

__attribute__((weak)) bool is_charge_enable_active(void) {
    // Default: return false (not charging)
    return false;
}

__attribute__((weak)) float get_tractive_voltage(void) {
    // Default: return 0V (safe default)
    return 0.0f;
}

__attribute__((weak)) float get_pack_voltage(void) {
    // Default: return pack voltage (read from BMS) in volts
    extern uint32_t getPackVoltage_mv(void);
    uint32_t pack_mv = getPackVoltage_mv();
    return ((float)pack_mv) / 1000.0f;
}
