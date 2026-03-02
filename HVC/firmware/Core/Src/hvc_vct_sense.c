/**
 ******************************************************************************
 * @file    hvc_vct_sense.c
 * @brief   HVC Voltage and Current Sensing Implementation
 * @details Implements voltage and current sensing for the HVC system
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 Longhorn Racing Electric
 * All rights reserved.
 *
 *
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "hvc_vct_sense.h"
#include "adc.h"
#include "longhorn/rtos/logger.h"

#define VSENSE_RATIO (2000750.0f / 750.0f / 8.2f)


float get_tractive_voltage(void)
{
    float tractive_voltage_adc_3v3 = (hvc_adc_read_voltage_sense_v() - 1.68f) * 2.0f;
    return tractive_voltage_adc_3v3 * VSENSE_RATIO;
}


float get_tractive_current(void)
{
    uint16_t raw = hvc_adc_read_current_sense_raw();
    return 0;
    // float v_adc = raw_u_to_v(raw);
    // return (v_adc - g_cfg.current_offset_v) * g_cfg.current_gain_a_per_v;
}
