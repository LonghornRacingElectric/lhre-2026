/**
 ******************************************************************************
 * @file    hvc_faults.c
 * @brief   HVC Faults implementation
 * @details 
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 Longhorn Racing Electric
 * All rights reserved.
 *
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "hvc_faults.h"
#include "main.h"
#include "longhorn/rtos/logger.h"
#include "hvc_bms.h"
/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/


float fault_timer_bms_comms = 0.0f;
float fault_timer_bms_overvoltage = 0.0f;
float fault_timer_bms_undervoltage = 0.0f;
float fault_timer_bms_overtemp = 0.0f;
float fault_timer_pack_overvoltage = 0.0f;
float fault_timer_vsense_implausible = 0.0f;
float fault_timer_overcurrent = 0.0f;
float fault_timer_isense_implausible = 0.0f;


void faults_init() {
    
}


uint32_t get_faults(float delta_time_s) {
    uint32_t current_fault_vector = 0;

    // BMS Communications Fault
    if (bms_check_disconnection()) {
        fault_timer_bms_comms += delta_time_s;
        if (fault_timer_bms_comms >= TIMER_BMS_COMMS) {
            current_fault_vector |= FAULT_BMS_COMMS;
        }
    } else {
        fault_timer_bms_comms = 0.0f;
    }

    // BMS Overvoltage Fault
    if (bms_check_overvoltage()) {
        fault_timer_bms_overvoltage += delta_time_s;
        if (fault_timer_bms_overvoltage >= TIMER_BMS_OVERVOLTAGE) {
            current_fault_vector |= FAULT_BMS_OVERVOLTAGE;
        }
    } else {
        fault_timer_bms_overvoltage = 0.0f;
    }

    // BMS Undervoltage Fault
    if (bms_check_undervoltage()) {
        fault_timer_bms_undervoltage += delta_time_s;
        if (fault_timer_bms_undervoltage >= TIMER_BMS_UNDERVOLTAGE) {
            current_fault_vector |= FAULT_BMS_UNDERVOLTAGE;
        }
    } else {
        fault_timer_bms_undervoltage = 0.0f;
    }

    // BMS Overtemp Fault
    if (bms_check_overtemp()) {
        fault_timer_bms_overtemp += delta_time_s;
        if (fault_timer_bms_overtemp >= TIMER_BMS_OVERTEMP) {
            current_fault_vector |= FAULT_BMS_OVERTEMP;
        }
    } else {
        fault_timer_bms_overtemp = 0.0f;
    }

    
    // TODO unimplemented faults

    // IMD

    // // Pack Overvoltage Fault
    // if (/*TODO: Pack overvoltage fault condition*/) {
    //     fault_timer_pack_overvoltage += delta_time_s;
    //     if (fault_timer_pack_overvoltage >= TIMER_PACK_OVERVOLTAGE) {
    //         current_fault_vector |= FAULT_PACK_OVERVOLTAGE;
    //     }
    // } else {
    //     fault_timer_pack_overvoltage = 0.0f;
    // }

    // // Vsense Implausible Fault
    // if (/*TODO: Vsense implausible fault condition*/) {
    //     fault_timer_vsense_implausible += delta_time_s;
    //     if (fault_timer_vsense_implausible >= TIMER_VSENSE_IMPLAUSIBLE) {
    //         current_fault_vector |= FAULT_VSENSE_IMPLAUSIBLE;
    //     }
    // } else {
    //     fault_timer_vsense_implausible = 0.0f;
    // }

    // // Overcurrent Fault
    // if (/*TODO: Overcurrent fault condition*/) {
    //     fault_timer_overcurrent += delta_time_s;
    //     if (fault_timer_overcurrent >= TIMER_OVERCURRENT) {
    //         current_fault_vector |= FAULT_OVERCURRENT;
    //     }
    // } else {
    //     fault_timer_overcurrent = 0.0f;
    // }

    // // Isense Implausible Fault
    // if (/*TODO: Isense implausible fault condition*/) {
    //     fault_timer_isense_implausible += delta_time_s;
    //     if (fault_timer_isense_implausible >= TIMER_ISENSE_IMPLAUSIBLE) {
    //         current_fault_vector |= FAULT_ISENSE_IMPLAUSIBLE;
    //     }
    // } else {
    //     fault_timer_isense_implausible = 0.0f;
    // }
    
    // // Contactor P Welded Fault
    // ...

    // // Contactor M Welded Fault
    // ...

    return current_fault_vector;
}


void set_bms_fault_pin(bool faulted) {
    HAL_GPIO_WritePin(BMS_Error_GPIO_Port, BMS_Error_Pin, faulted ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
