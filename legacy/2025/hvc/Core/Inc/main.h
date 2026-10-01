/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
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
#define TEMP_ADC3_Pin GPIO_PIN_0
#define TEMP_ADC3_GPIO_Port GPIOC
#define TEMP_ADC4_Pin GPIO_PIN_1
#define TEMP_ADC4_GPIO_Port GPIOC
#define IR_POS_SENSE_Pin GPIO_PIN_2
#define IR_POS_SENSE_GPIO_Port GPIOC
#define IR_NEG_SENSE_Pin GPIO_PIN_3
#define IR_NEG_SENSE_GPIO_Port GPIOC
#define LED_G_Pin GPIO_PIN_0
#define LED_G_GPIO_Port GPIOA
#define LED_R_Pin GPIO_PIN_1
#define LED_R_GPIO_Port GPIOA
#define LED_B_Pin GPIO_PIN_2
#define LED_B_GPIO_Port GPIOA
#define TEMP_ADC1_Pin GPIO_PIN_3
#define TEMP_ADC1_GPIO_Port GPIOA
#define TEMP_ADC2_Pin GPIO_PIN_4
#define TEMP_ADC2_GPIO_Port GPIOA
#define I_SENSE_P_Pin GPIO_PIN_6
#define I_SENSE_P_GPIO_Port GPIOA
#define I_SENSE_N_Pin GPIO_PIN_7
#define I_SENSE_N_GPIO_Port GPIOA
#define V_SENSE_P_Pin GPIO_PIN_4
#define V_SENSE_P_GPIO_Port GPIOC
#define V_SENSE_N_Pin GPIO_PIN_5
#define V_SENSE_N_GPIO_Port GPIOC
#define BMS_ERROR_Pin GPIO_PIN_0
#define BMS_ERROR_GPIO_Port GPIOB
#define IMD_ERROR_Pin GPIO_PIN_1
#define IMD_ERROR_GPIO_Port GPIOB
#define Shutdown_Sense_12_Pin GPIO_PIN_8
#define Shutdown_Sense_12_GPIO_Port GPIOA
#define VCP_RX_Pin GPIO_PIN_12
#define VCP_RX_GPIO_Port GPIOB
#define VCP_TX_Pin GPIO_PIN_13
#define VCP_TX_GPIO_Port GPIOB
#define CS_BMB_Pin GPIO_PIN_9
#define CS_BMB_GPIO_Port GPIOD
#define IMD_CAN_RX_Pin GPIO_PIN_12
#define IMD_CAN_RX_GPIO_Port GPIOD
#define IMD_CAN_TX_Pin GPIO_PIN_13
#define IMD_CAN_TX_GPIO_Port GPIOD
#define SHDN_SENSE_1_Pin GPIO_PIN_6
#define SHDN_SENSE_1_GPIO_Port GPIOC
#define SHDN_SENSE_2_Pin GPIO_PIN_7
#define SHDN_SENSE_2_GPIO_Port GPIOC
#define SHDN_SENSE_3_Pin GPIO_PIN_8
#define SHDN_SENSE_3_GPIO_Port GPIOC
#define SHDN_SENSE_4_Pin GPIO_PIN_9
#define SHDN_SENSE_4_GPIO_Port GPIOC
#define BOOT0trig_Pin GPIO_PIN_15
#define BOOT0trig_GPIO_Port GPIOA
#define SCK_BMB_Pin GPIO_PIN_10
#define SCK_BMB_GPIO_Port GPIOC
#define MISO_BMB_Pin GPIO_PIN_11
#define MISO_BMB_GPIO_Port GPIOC
#define NW_CAN_RX_Pin GPIO_PIN_0
#define NW_CAN_RX_GPIO_Port GPIOD
#define NW_CAN_TX_Pin GPIO_PIN_1
#define NW_CAN_TX_GPIO_Port GPIOD
#define MOSI_BMB_Pin GPIO_PIN_6
#define MOSI_BMB_GPIO_Port GPIOD
#define CLOSE_IR_POS_Pin GPIO_PIN_6
#define CLOSE_IR_POS_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
#define IMD_Data_GPIO_Port GPIOA
#define IMD_Data_Pin GPIO_PIN_9 // IMD_OK Pin
#define Precharge_Enable_Pin GPIO_PIN_1
#define Precharge_Enable_GPIO_Port GPIOB
#define Close_HV_P_Signal_Pin GPIO_PIN_2
#define Close_HV_P_Signal_GPIO_Port GPIOB

#ifndef USB_VCP
#define USB_VCP
#define SELF_BOOT_DFU
#endif
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
