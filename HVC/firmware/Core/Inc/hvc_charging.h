/**
 ******************************************************************************
 * @file    hvc_charging.h
 * @brief   HVC Charging Control Module Header
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 Longhorn Racing Electric
 * All rights reserved.
 *
 ******************************************************************************
 */

#ifndef HVC_CHARGING_H
#define HVC_CHARGING_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Run one charging control iteration (call at 10 Hz from HVC_STATE_CHARGING).
 *
 * Computes a voltage setpoint that tapers from the full 546 V pack limit down
 * to the actual measured pack voltage as the highest cell approaches 4.2 V,
 * giving the charger a smooth entry into CV mode.  Charge current is also
 * reduced proportionally to cell-voltage spread so active balancing can keep
 * pace with the incoming energy.
 */
void hvc_control_charging(bool enable);

#ifdef __cplusplus
}
#endif

#endif /* HVC_CHARGING_H */
