/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l4xx_hal.h"

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
#define SW_RadFan_Pin GPIO_PIN_13
#define SW_RadFan_GPIO_Port GPIOC
#define CAN_Term_Pin GPIO_PIN_14
#define CAN_Term_GPIO_Port GPIOC
#define LED_R_Pin GPIO_PIN_0
#define LED_R_GPIO_Port GPIOA
#define LED_G_Pin GPIO_PIN_1
#define LED_G_GPIO_Port GPIOA
#define LED_B_Pin GPIO_PIN_2
#define LED_B_GPIO_Port GPIOA
#define Fault_SHDN_Pin GPIO_PIN_3
#define Fault_SHDN_GPIO_Port GPIOA
#define Fault_GLV_Pin GPIO_PIN_4
#define Fault_GLV_GPIO_Port GPIOA
#define Fault_BattFan_Pin GPIO_PIN_5
#define Fault_BattFan_GPIO_Port GPIOA
#define Fault_RadFan_Pin GPIO_PIN_6
#define Fault_RadFan_GPIO_Port GPIOA
#define Fault_BrakeLight_Pin GPIO_PIN_7
#define Fault_BrakeLight_GPIO_Port GPIOA
#define Fault_Pump2_Pin GPIO_PIN_0
#define Fault_Pump2_GPIO_Port GPIOB
#define Tach_FlowRate_Pin GPIO_PIN_1
#define Tach_FlowRate_GPIO_Port GPIOB
#define Fault_Pump1_Pin GPIO_PIN_2
#define Fault_Pump1_GPIO_Port GPIOB
#define SPI_SCK_Pin GPIO_PIN_10
#define SPI_SCK_GPIO_Port GPIOB
#define SPI_CS_ADC_Pin GPIO_PIN_11
#define SPI_CS_ADC_GPIO_Port GPIOB
#define SPI_CS_IMU_Pin GPIO_PIN_12
#define SPI_CS_IMU_GPIO_Port GPIOB
#define SPI_CS_EEPROM_Pin GPIO_PIN_13
#define SPI_CS_EEPROM_GPIO_Port GPIOB
#define SPI_MISO_Pin GPIO_PIN_14
#define SPI_MISO_GPIO_Port GPIOB
#define SPI_MOSI_Pin GPIO_PIN_15
#define SPI_MOSI_GPIO_Port GPIOB
#define PWM_Pump1_Pin GPIO_PIN_8
#define PWM_Pump1_GPIO_Port GPIOA
#define PWM_Pump2_Pin GPIO_PIN_9
#define PWM_Pump2_GPIO_Port GPIOA
#define PWM_BrakeLight_Pin GPIO_PIN_10
#define PWM_BrakeLight_GPIO_Port GPIOA
#define PWM_RadFan_Pin GPIO_PIN_11
#define PWM_RadFan_GPIO_Port GPIOA
#define SW_BattFan_Pin GPIO_PIN_12
#define SW_BattFan_GPIO_Port GPIOA
#define SW_GLV_Pin GPIO_PIN_15
#define SW_GLV_GPIO_Port GPIOA
#define SW_SHDN_Pin GPIO_PIN_4
#define SW_SHDN_GPIO_Port GPIOB
#define Tach_RadFan_Pin GPIO_PIN_5
#define Tach_RadFan_GPIO_Port GPIOB
#define RTD_Enable_Pin GPIO_PIN_6
#define RTD_Enable_GPIO_Port GPIOB
#define Button_Pin GPIO_PIN_7
#define Button_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
