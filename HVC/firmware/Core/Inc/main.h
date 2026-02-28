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
#define SPI4_CS_Pin GPIO_PIN_3
#define SPI4_CS_GPIO_Port GPIOE
#define Temp_Sense_3_Pin GPIO_PIN_0
#define Temp_Sense_3_GPIO_Port GPIOC
#define Temp_Sense_4_Pin GPIO_PIN_1
#define Temp_Sense_4_GPIO_Port GPIOC
#define IR__Sense_Pin GPIO_PIN_2
#define IR__Sense_GPIO_Port GPIOC
#define IR__SenseC3_Pin GPIO_PIN_3
#define IR__SenseC3_GPIO_Port GPIOC
#define Temp_Sense_1_Pin GPIO_PIN_3
#define Temp_Sense_1_GPIO_Port GPIOA
#define Temp_Sense_2_Pin GPIO_PIN_4
#define Temp_Sense_2_GPIO_Port GPIOA
#define Current_Sense___Pin GPIO_PIN_6
#define Current_Sense___GPIO_Port GPIOA
#define Current_Sense__A7_Pin GPIO_PIN_7
#define Current_Sense__A7_GPIO_Port GPIOA
#define Voltage_Sense___Pin GPIO_PIN_4
#define Voltage_Sense___GPIO_Port GPIOC
#define Voltage_Sense__C5_Pin GPIO_PIN_5
#define Voltage_Sense__C5_GPIO_Port GPIOC
#define BMS_Error_Pin GPIO_PIN_1
#define BMS_Error_GPIO_Port GPIOB
#define IMD_Error_Pin GPIO_PIN_2
#define IMD_Error_GPIO_Port GPIOB
#define Shutdown_Sense_1_Pin GPIO_PIN_6
#define Shutdown_Sense_1_GPIO_Port GPIOC
#define Shutdown_Sense_2_Pin GPIO_PIN_7
#define Shutdown_Sense_2_GPIO_Port GPIOC
#define Shutdown_Sense_3_Pin GPIO_PIN_8
#define Shutdown_Sense_3_GPIO_Port GPIOC
#define Shutdown_Sense_4_Pin GPIO_PIN_9
#define Shutdown_Sense_4_GPIO_Port GPIOC
#define Shutdown_Sense_12_Pin GPIO_PIN_8
#define Shutdown_Sense_12_GPIO_Port GPIOA
#define Close_IR___Pin GPIO_PIN_6
#define Close_IR___GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
