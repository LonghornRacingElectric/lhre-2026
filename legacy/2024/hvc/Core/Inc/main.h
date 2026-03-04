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
#include "stm32h7xx_hal.h"

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
#define TSV_Pin GPIO_PIN_13
#define TSV_GPIO_Port GPIOC
#define Calibrate_Button_Pin GPIO_PIN_15
#define Calibrate_Button_GPIO_Port GPIOC
#define Prox_to_MCU_Pin GPIO_PIN_0
#define Prox_to_MCU_GPIO_Port GPIOC
#define A_HV_to_MCU_Pin GPIO_PIN_1
#define A_HV_to_MCU_GPIO_Port GPIOC
#define A_HV_lowCurr_to_MCU_Pin GPIO_PIN_2
#define A_HV_lowCurr_to_MCU_GPIO_Port GPIOC
#define A_HV_highCurr_to_MCU_Pin GPIO_PIN_3
#define A_HV_highCurr_to_MCU_GPIO_Port GPIOC
#define LED_B_Pin GPIO_PIN_0
#define LED_B_GPIO_Port GPIOA
#define LED_R_Pin GPIO_PIN_1
#define LED_R_GPIO_Port GPIOA
#define LED_G_Pin GPIO_PIN_2
#define LED_G_GPIO_Port GPIOA
#define Precharge_Enable_Pin GPIO_PIN_3
#define Precharge_Enable_GPIO_Port GPIOA
#define SCK_Telem_Pin GPIO_PIN_5
#define SCK_Telem_GPIO_Port GPIOA
#define MISO_Telem_Pin GPIO_PIN_6
#define MISO_Telem_GPIO_Port GPIOA
#define Contactor_PGood_Signal_Pin GPIO_PIN_7
#define Contactor_PGood_Signal_GPIO_Port GPIOA
#define Close_HV_N_Signal_Pin GPIO_PIN_4
#define Close_HV_N_Signal_GPIO_Port GPIOC
#define Close_HV_P_Signal_Pin GPIO_PIN_5
#define Close_HV_P_Signal_GPIO_Port GPIOC
#define Ctrl_from_MCU_Pin GPIO_PIN_0
#define Ctrl_from_MCU_GPIO_Port GPIOB
#define Ctrl_to_MCU_Pin GPIO_PIN_1
#define Ctrl_to_MCU_GPIO_Port GPIOB
#define SCK_BMS_Pin GPIO_PIN_10
#define SCK_BMS_GPIO_Port GPIOB
#define CAN_RX_Charge_Pin GPIO_PIN_12
#define CAN_RX_Charge_GPIO_Port GPIOB
#define CAN_TX_Charge_Pin GPIO_PIN_13
#define CAN_TX_Charge_GPIO_Port GPIOB
#define MISO_BMS_Pin GPIO_PIN_14
#define MISO_BMS_GPIO_Port GPIOB
#define MOSI_BMS_Pin GPIO_PIN_15
#define MOSI_BMS_GPIO_Port GPIOB
#define Term_Enable_Charge_Pin GPIO_PIN_6
#define Term_Enable_Charge_GPIO_Port GPIOC
#define AMS_Error_Pin GPIO_PIN_7
#define AMS_Error_GPIO_Port GPIOC
#define IMD_Data_Pin GPIO_PIN_9
#define IMD_Data_GPIO_Port GPIOA
#define CAN_Term_Pin GPIO_PIN_10
#define CAN_Term_GPIO_Port GPIOA
#define CAN_RX_MCU_Pin GPIO_PIN_11
#define CAN_RX_MCU_GPIO_Port GPIOA
#define CAN_TX_MCU_Pin GPIO_PIN_12
#define CAN_TX_MCU_GPIO_Port GPIOA
#define SPI_CS_BMS_Pin GPIO_PIN_10
#define SPI_CS_BMS_GPIO_Port GPIOC
#define SPI_CS_EEPROM_Pin GPIO_PIN_11
#define SPI_CS_EEPROM_GPIO_Port GPIOC
#define SPI_CS_TEMP_Pin GPIO_PIN_12
#define SPI_CS_TEMP_GPIO_Port GPIOC
#define SPI_CS_IMU_Pin GPIO_PIN_2
#define SPI_CS_IMU_GPIO_Port GPIOD
#define MOSI_Telem_Pin GPIO_PIN_5
#define MOSI_Telem_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
