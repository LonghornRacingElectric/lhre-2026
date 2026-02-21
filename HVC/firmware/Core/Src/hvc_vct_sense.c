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

/*
 * Legacy (2025) differential formula:
 *   V_tractive = (raw_vsense_v - COMMON_MODE_VOLTAGE) / VOLTAGE_CONSTANT * 2
 * where VOLTAGE_CONSTANT = (7.95 * 750 / 2000750)
 *
 * This year's implementation converts ADC code -> Vadc (Volts) first, then applies
 * configurable scaling. We keep the legacy constants here for traceability.
 */
#define HVC_LEGACY_COMMON_MODE_VOLTAGE_V  (1.735052f)
#define HVC_LEGACY_VOLTAGE_CONSTANT       (7.95f * 750.0f / 2000750.0f)

/* Hardware scaling notes:
 * - Divider: Rtop = 2.0 MOhm, Rbot = 750 Ohm => V_batt = V_adc * ((Rtop + Rbot) / Rbot)
 * - Amplifier gain: 7.95 (if the ADC is measuring the amplified divider node)
 *
 * If your signal chain is: V_batt -> divider -> amplifier (gain 7.95) -> ADC,
 * then: V_batt = V_adc * ((Rtop + Rbot) / Rbot) / 7.95
 */
#define HVC_VSENSE_RTOP_OHM   (2000000.0f)
#define HVC_VSENSE_RBOT_OHM   (750.0f)
#define HVC_VSENSE_AMP_GAIN   (7.95f)
#define HVC_VSENSE_DIV_RATIO  ((HVC_VSENSE_RTOP_OHM + HVC_VSENSE_RBOT_OHM) / HVC_VSENSE_RBOT_OHM)

static hvc_vct_sense_config_t g_cfg = {
    .vref_v = 3.3f,
    .adc_max = 4095,

    /* Default tractive voltage scaling from measured ADC voltage */
    .voltage_divider_ratio = (HVC_VSENSE_DIV_RATIO / HVC_VSENSE_AMP_GAIN),

    /* Empirical offset: at 0V input, raw diff is ~210-220 counts */
    .voltage_offset_raw = 215,
    .voltage_offset_v = 0.0f,

    /* Keep legacy common-mode/gain as placeholders for current conversion until calibrated */
    .current_offset_v = HVC_LEGACY_COMMON_MODE_VOLTAGE_V,
    .current_gain_a_per_v = (2.0f / HVC_LEGACY_VOLTAGE_CONSTANT),
};

void hvc_vct_sense_set_config(hvc_vct_sense_config_t cfg)
{
    g_cfg = cfg;
    if (g_cfg.adc_max == 0) {
        g_cfg.adc_max = 4095;
    }
}

hvc_vct_sense_config_t hvc_vct_sense_get_config(void)
{
    return g_cfg;
}

static float raw_to_v(uint16_t raw)
{
    if (g_cfg.adc_max == 0) {
        return 0.0f;
    }

    /* raw may be a signed-differential bit-pattern (see hvc_adc_read_voltage_sense_raw). */
    int16_t sraw = (int16_t)raw;
    return ((float)sraw * g_cfg.vref_v) / (float)g_cfg.adc_max;
}

static float raw_u_to_v(uint16_t raw)
{
    if (g_cfg.adc_max == 0) {
        return 0.0f;
    }
    return ((float)raw * g_cfg.vref_v) / (float)g_cfg.adc_max;
}

float get_tractive_voltage(void)
{
    uint16_t raw = hvc_adc_read_voltage_sense_raw();

    /* raw is treated as an unsigned code representing (pos-neg) after any packing.
     * Subtract measured zero offset in raw-code domain.
     */
    int32_t corrected = (int32_t)raw - (int32_t)g_cfg.voltage_offset_raw;
    if (corrected < 0) {
        corrected = 0;
    }

    float v_adc = raw_u_to_v((uint16_t)corrected);
    v_adc -= g_cfg.voltage_offset_v;

    return v_adc * g_cfg.voltage_divider_ratio;
}

float get_tractive_current(void)
{
    uint16_t raw = hvc_adc_read_current_sense_raw();
    float v_adc = raw_u_to_v(raw);
    return (v_adc - g_cfg.current_offset_v) * g_cfg.current_gain_a_per_v;
}
