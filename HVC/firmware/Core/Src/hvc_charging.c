/**
 ******************************************************************************
 * @file    hvc_charging.c
 * @brief   HVC Charging Control Implementation
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 Longhorn Racing Electric
 * All rights reserved.
 *
 ******************************************************************************
 */

#include "hvc_charging.h"
#include "hvc_bms.h"
#include "hvc_faults.h"
#include "can.h"

/* ---- Pack geometry --------------------------------------------------------*/
#define NUM_SERIES_CELLS     130
#define CELL_MAX_V           4.2f
#define MAX_PACK_VOLTAGE_V   546.0f

/* ---- Charging limits ------------------------------------------------------*/
// Cell voltage at which we begin tapering the commanded pack voltage.
// Below this threshold the charger operates in full CC mode.
#define CELL_CV_START_V      4.1f

// Absolute maximum charge current commanded to the charger.
// #define MAX_CHARGE_CURRENT_A 15.0f
#define MAX_CHARGE_CURRENT_A 9.5f


/* ---- Private helpers ------------------------------------------------------*/

static inline float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* ---- Exported functions ---------------------------------------------------*/

void hvc_control_charging(bool enable) {
    float max_cell   = bms_get_max_voltage();
    float min_cell   = bms_get_min_voltage();
    float pack_v     = bms_get_pack_voltage();
    float cell_delta = max_cell - min_cell;

    // CV taper ---------------------------------------------------------------
    float t = clampf(
        (CELL_MAX_V - max_cell) / (CELL_MAX_V - CELL_CV_START_V),
        0.0f, 1.0f);
    float commanded_v = MAX_PACK_VOLTAGE_V;// + t * (pack_v - MAX_PACK_VOLTAGE_V);
    float commanded_a = MAX_CHARGE_CURRENT_A * t;

    // LED / fault states for the charger -------------------------------------
    bool bms_led = get_latched_faults() != 0;
    bool imd_led = hvc_gpio_is_imd_error_active();

    hvc_set_charger_command(commanded_v, commanded_a, imd_led, bms_led, enable);
}
