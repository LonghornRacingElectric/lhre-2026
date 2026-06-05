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

/** Get tractive/battery voltage (Volts) using ADC voltage sense channel. */
float get_tractive_voltage(void);

/** Get tractive current (Amps) using ADC current sense channel. */
float get_tractive_current(void);

#ifdef __cplusplus
}
#endif

#endif /* HVC_VCT_SENSE_H */