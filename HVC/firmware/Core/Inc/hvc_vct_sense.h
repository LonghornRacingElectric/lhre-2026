/**
 ******************************************************************************
 * @file    hvc_vct_sense.h
 * @brief   HVC Voltage and Current Sensing Module Header
 * @details High voltage sensing for battery voltage and current measurements.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 Longhorn Racing Electric
 * All rights reserved.
 *
 ******************************************************************************
 */

#ifndef HVC_VCT_SENSE_H
#define HVC_VCT_SENSE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float vref_v;              /* ADC reference voltage (e.g. 3.3) */
    uint16_t adc_max;          /* ADC full-scale code (e.g. 4095 for 12-bit) */

    /* Voltage scaling */
    float voltage_divider_ratio;

    /* Offset correction for voltage measurement.
     * Use either raw-code offset or voltage-domain offset.
     */
    uint16_t voltage_offset_raw; /* raw code to subtract before conversion */
    float voltage_offset_v;      /* volts to subtract after conversion (optional) */

    /* Current conversion */
    float current_offset_v;    /* sensor output at 0A */
    float current_gain_a_per_v;/* A per Volt at ADC pin */
} hvc_vct_sense_config_t;

/** Set the conversion parameters used by the getters below. */
void hvc_vct_sense_set_config(hvc_vct_sense_config_t cfg);

/** Get the currently configured conversion parameters. */
hvc_vct_sense_config_t hvc_vct_sense_get_config(void);

/** Get tractive/battery voltage (Volts) using ADC voltage sense channel. */
float get_tractive_voltage(void);

/** Get tractive current (Amps) using ADC current sense channel. */
float get_tractive_current(void);

#ifdef __cplusplus
}
#endif

#endif /* HVC_VCT_SENSE_H */