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
#define LATCH_TSSI_G_Pin GPIO_PIN_15
#define LATCH_TSSI_G_GPIO_Port GPIOC
#define SW_PUMPS_Pin GPIO_PIN_10
#define SW_PUMPS_GPIO_Port GPIOF
#define EN_SDWN_Pin GPIO_PIN_0
#define EN_SDWN_GPIO_Port GPIOC
#define SW_SPARE_Pin GPIO_PIN_1
#define SW_SPARE_GPIO_Port GPIOC
#define SW_BOARDS_Pin GPIO_PIN_2
#define SW_BOARDS_GPIO_Port GPIOC
#define SW_FANS_Pin GPIO_PIN_3
#define SW_FANS_GPIO_Port GPIOC
#define Line_Lock_EN_Pin GPIO_PIN_2
#define Line_Lock_EN_GPIO_Port GPIOF
#define PWM_TSSI_R_Pin GPIO_PIN_0
#define PWM_TSSI_R_GPIO_Port GPIOA
#define PWM_BRAKEL_Pin GPIO_PIN_1
#define PWM_BRAKEL_GPIO_Port GPIOA
#define PWM_TSSI_G_Pin GPIO_PIN_2
#define PWM_TSSI_G_GPIO_Port GPIOA
#define SNS_TSSI_G_Pin GPIO_PIN_0
#define SNS_TSSI_G_GPIO_Port GPIOB
#define ST_TSSI_G_Pin GPIO_PIN_15
#define ST_TSSI_G_GPIO_Port GPIOE
#define SDWN_12_Sense_Pin GPIO_PIN_14
#define SDWN_12_Sense_GPIO_Port GPIOB
#define SDWN_13_Sense_Pin GPIO_PIN_15
#define SDWN_13_Sense_GPIO_Port GPIOB
#define SDWN_11_Sense_Pin GPIO_PIN_8
#define SDWN_11_Sense_GPIO_Port GPIOD
#define SDWN_15_Sense_Pin GPIO_PIN_9
#define SDWN_15_Sense_GPIO_Port GPIOC
#define SDWN_1_Sense_Pin GPIO_PIN_8
#define SDWN_1_Sense_GPIO_Port GPIOA
#define BSE_3_Pin GPIO_PIN_9
#define BSE_3_GPIO_Port GPIOA
#define SDWN_14_Sense_Pin GPIO_PIN_15
#define SDWN_14_Sense_GPIO_Port GPIOA
#define SW_BATT_FANS1_Pin GPIO_PIN_4
#define SW_BATT_FANS1_GPIO_Port GPIOB
#define SW_BATT_FANS2_Pin GPIO_PIN_5
#define SW_BATT_FANS2_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
