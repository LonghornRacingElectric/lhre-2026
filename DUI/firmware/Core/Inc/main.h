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
#define BMS_Error_SW_Pin GPIO_PIN_0
#define BMS_Error_SW_GPIO_Port GPIOB
#define IMD_Error_SW_Pin GPIO_PIN_1
#define IMD_Error_SW_GPIO_Port GPIOB
#define Switch_Top_L_UP_Pin GPIO_PIN_7
#define Switch_Top_L_UP_GPIO_Port GPIOE
#define Switch_Top_L_DOWN_Pin GPIO_PIN_8
#define Switch_Top_L_DOWN_GPIO_Port GPIOE
#define Switch_Top_R_UP_Pin GPIO_PIN_9
#define Switch_Top_R_UP_GPIO_Port GPIOE
#define Switch_Top_R_DOWN_Pin GPIO_PIN_10
#define Switch_Top_R_DOWN_GPIO_Port GPIOE
#define Switch_Bottom_R_UP_Pin GPIO_PIN_11
#define Switch_Bottom_R_UP_GPIO_Port GPIOE
#define Switch_Bottom_L_DOWN_Pin GPIO_PIN_12
#define Switch_Bottom_L_DOWN_GPIO_Port GPIOE
#define Switch_Bottom_R_DOWN_Pin GPIO_PIN_13
#define Switch_Bottom_R_DOWN_GPIO_Port GPIOE
#define Switch_Bottom_L_UP_Pin GPIO_PIN_14
#define Switch_Bottom_L_UP_GPIO_Port GPIOE
#define SDWN_Sense_Inertia_SW_Pin GPIO_PIN_15
#define SDWN_Sense_Inertia_SW_GPIO_Port GPIOE
#define SDSW_Sense_EStop_Pin GPIO_PIN_10
#define SDSW_Sense_EStop_GPIO_Port GPIOB
#define RTD_SWITCH_Pin GPIO_PIN_11
#define RTD_SWITCH_GPIO_Port GPIOB
#define SPEAKER_Pin GPIO_PIN_15
#define SPEAKER_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
