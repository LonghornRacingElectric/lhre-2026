/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.h
  * @brief   This file contains all the function prototypes for
  *          the gpio.c file
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
#ifndef __GPIO_H__
#define __GPIO_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */
#include <stdbool.h>
/* USER CODE END Includes */

/* USER CODE BEGIN Private defines */
#define BSPD_BrakePressed_Pin GPIO_PIN_13
#define BSPD_BrakePressed_GPIO_Port GPIOC
#define BSPD_MotorOn_Pin GPIO_PIN_14
#define BSPD_MotorOn_GPIO_Port GPIOC
#define BSPD_Shutdown_Pin GPIO_PIN_15
#define BSPD_Shutdown_GPIO_Port GPIOC
#define BSPD_BrakeFailure_Pin GPIO_PIN_3
#define BSPD_BrakeFailure_GPIO_Port GPIOC
#define BSPD_MotorFailure_Pin GPIO_PIN_2
#define BSPD_MotorFailure_GPIO_Port GPIOB

typedef struct BSPD {
    bool brake_pressed;
    bool motor_on;
    bool shutdown;
    bool brake_failure;
    bool motor_failure;
} BSPD;

/* USER CODE END Private defines */

void MX_GPIO_Init(void);

/* USER CODE BEGIN Prototypes */
/**
 * Get the state of the BSPD to be sent to telemetry.
 * @param bspd_state
 */
void gpio_get_bspd(BSPD* bspd_state);

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif
#endif /*__ GPIO_H__ */

