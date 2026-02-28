/**
 ******************************************************************************
 * @file    hvc_faults.h
 * @brief   HVC faults module
 * @details All faults
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 Longhorn Racing Electric
 * All rights reserved.
 *
 ******************************************************************************
 */

#ifndef HVC_FAULTS_H
#define HVC_FAULTS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>


/* Defintions ----------------------------------------------------------------*/
#define FAULT_BMS_COMMS             (1 << 0)
#define FAULT_BMS_OVERVOLTAGE       (1 << 1)
#define FAULT_BMS_UNDERVOLTAGE      (1 << 2)
#define FAULT_BMS_OVERTEMP          (1 << 3)
#define FAULT_PACK_OVERVOLTAGE      (1 << 4)
#define FAULT_VSENSE_IMPLAUSIBLE    (1 << 5)
#define FAULT_OVERCURRENT           (1 << 6)
#define FAULT_ISENSE_IMPLAUSIBLE    (1 << 7)
#define FAULT_CONTACTOR_P_WELDED    (1 << 8)
#define FAULT_CONTACTOR_M_WELDED    (1 << 9)

#define TIMER_BMS_COMMS             2.0f
#define TIMER_BMS_OVERVOLTAGE       10.0f
#define TIMER_BMS_UNDERVOLTAGE      10.0f
#define TIMER_BMS_OVERTEMP          5.0f
#define TIMER_PACK_OVERVOLTAGE      10.0f
#define TIMER_VSENSE_IMPLAUSIBLE    1.0f
#define TIMER_OVERCURRENT           2.0f
#define TIMER_ISENSE_IMPLAUSIBLE    1.0f
#define TIMER_CONTACTOR_P_WELDED    1.0f
#define TIMER_CONTACTOR_M_WELDED    1.0f

/* Exported functions --------------------------------------------------------*/

/**
 * @brief Initialize faults module
 * @note
 */
void faults_init();

/**
 * @brief Check for faults
 * @return fault bitfield
 */
uint32_t get_faults(float delta_time_s);

#ifdef __cplusplus
}
#endif

#endif /* HVC_FAULTS_H */
