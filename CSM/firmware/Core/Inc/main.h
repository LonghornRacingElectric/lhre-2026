/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define IMU_INT1_Pin GPIO_PIN_13
#define IMU_INT1_GPIO_Port GPIOC
#define IMU_INT1_EXTI_IRQn EXTI15_10_IRQn
#define IMU_INT2_Pin GPIO_PIN_14
#define IMU_INT2_GPIO_Port GPIOC
#define IMU_INT2_EXTI_IRQn EXTI15_10_IRQn
#define STRAIN_GAUGE_VA_IN_Pin GPIO_PIN_3
#define STRAIN_GAUGE_VA_IN_GPIO_Port GPIOA
#define RH_CS_Pin GPIO_PIN_0
#define RH_CS_GPIO_Port GPIOB
#define IMU_CS_Pin GPIO_PIN_1
#define IMU_CS_GPIO_Port GPIOB
#define SUS_POT_VWIPER_Pin GPIO_PIN_2
#define SUS_POT_VWIPER_GPIO_Port GPIOB
#define STRAIN_GAUGE_VB_IN_Pin GPIO_PIN_14
#define STRAIN_GAUGE_VB_IN_GPIO_Port GPIOB
#define RH_IRQ_Pin GPIO_PIN_15
#define RH_IRQ_GPIO_Port GPIOB
#define RH_IRQ_EXTI_IRQn EXTI15_10_IRQn
#define BOOT0trig_Pin GPIO_PIN_9
#define BOOT0trig_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
