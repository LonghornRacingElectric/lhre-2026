/**
 ******************************************************************************
 * @file    hvc_contactors.h
 * @brief   HVC Contactor Control Module Header
 * @details High voltage contactor control for precharge resistor and AIRs
 *          (Accumulator Isolation Relays). Based on 2024 implementation.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 Longhorn Racing Electric
 * All rights reserved.
 *
 ******************************************************************************
 */

#ifndef HVC_CONTACTORS_H
#define HVC_CONTACTORS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdbool.h>

/* Exported functions --------------------------------------------------------*/

/**
 * @brief Initialize contactor control
 * @note Configures GPIO pins for contactor control
 */
void contactors_init(void);

/**
 * @brief Set precharge contactor state
 * @param state true to close precharge contactor, false to open
 */
void set_precharge_contactor(bool state);

/**
 * @brief Set drive contactors state (AIR+ and AIR-)
 * @param state true to close both AIRs, false to open
 * @note Controls both positive and negative AIRs together
 */
void set_drive_contactors(bool state);

/**
 * @brief Emergency open all contactors
 * @note Opens all contactors immediately for safety
 */
void open_all_contactors(void);

/**
 * @brief Get precharge contactor state
 * @return true if closed, false if open
 */
bool get_precharge_contactor_state(void);

/**
 * @brief Get drive contactors state
 * @return true if closed, false if open
 */
bool get_drive_contactors_state(void);

#ifdef __cplusplus
}
#endif

#endif /* HVC_CONTACTORS_H */
