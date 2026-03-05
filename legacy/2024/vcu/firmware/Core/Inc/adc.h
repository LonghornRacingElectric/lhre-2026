/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    adc.h
  * @brief   This file contains all the function prototypes for
  *          the adc.c file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __ADC_H__
#define __ADC_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */
#include <stdbool.h>
/* USER CODE END Includes */

extern ADC_HandleTypeDef hadc1;

/* USER CODE BEGIN Private defines */

#define NUM_SENSORS 7
#define APPS1 0
#define APPS2 1
#define BSE1 2
#define BSE2 3
#define STEER 4
#define SUS1 5
#define SUS2 6

typedef struct AnalogVoltages {
  float apps1, apps2;
  float bse1, bse2;
  float steer;
  float sus1, sus2;
} AnalogVoltages;

volatile static uint16_t adcValues[NUM_SENSORS];

/* USER CODE END Private defines */

void MX_ADC1_Init(void);

/* USER CODE BEGIN Prototypes */
/**
 * Calibrate the offset for the analog sensors.
 * @param hadc1
 * @return error code
 */
static int adc_calibrate(ADC_HandleTypeDef* hadc1);

/**
 * Initialize the ADC and DMA for analog sensors.
 * @param hadc1
 * @return error code
 */
static int adc_start(ADC_HandleTypeDef* hadc1);

/**
 * Update the values in the struct with the latest voltages.
 * @param analogVoltages
 */
void adc_periodic(AnalogVoltages* analogVoltages);


/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __ADC_H__ */

