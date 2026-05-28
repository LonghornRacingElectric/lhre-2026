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

#define ADC_DIFF_MID_CODE 2048.0f
#define ADC_DIFF_LSB_V    (3.3f / 4096.0f)
#define ADC_DIFF_GAIN     2.0f

#define VSENSE_RATIO (2000750.0f / 750.0f / 8.2f)
#define CURRENT_SENSE_V_PER_A (8.5f * 0.001f) // 1mOhm shunt with 8.5 V/V gain

static float adc_diff_raw_to_v(uint16_t raw)
{
    return ((float)raw - ADC_DIFF_MID_CODE) * ADC_DIFF_LSB_V * ADC_DIFF_GAIN;
}

float get_tractive_voltage(void)
{
    float tractive_voltage_adc_3v3 = adc_diff_raw_to_v(hvc_adc_read_voltage_sense_raw());
    return tractive_voltage_adc_3v3 * VSENSE_RATIO;
}


float get_tractive_current(void)
{
    float tractive_current_adc_3v3 = adc_diff_raw_to_v(hvc_adc_read_current_sense_raw());
    return tractive_current_adc_3v3 / CURRENT_SENSE_V_PER_A;
}
