/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.h
  * @brief   This file contains all the function prototypes for
  *          the usart.c file
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
#ifndef __USART_H__
#define __USART_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */
#include <stdbool.h>

#define BUF_SIZE 4096
#define MAX_GPS_LINE_SIZE 4096 ///< how long are max NMEA lines to parse?
#define MAX_CELL_LINE_SIZE 4096 ///< how long are max AT lines to parse?
extern bool gps_completeLine;
extern char gps_currLine[MAX_GPS_LINE_SIZE];
extern char gps_tempLine[BUF_SIZE];
extern uint16_t gps_currLineSize;
extern bool cell_completeLine;
extern char cell_currLine[MAX_CELL_LINE_SIZE];
extern uint16_t cell_currLineSize;
extern char cell_tempLine[BUF_SIZE];


/* USER CODE END Includes */

extern UART_HandleTypeDef hlpuart1;

extern UART_HandleTypeDef huart4;

extern UART_HandleTypeDef huart7;

extern UART_HandleTypeDef huart1;

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

void MX_LPUART1_UART_Init(void);
void MX_UART4_Init(void);
void MX_UART7_Init(void);
void MX_USART1_UART_Init(void);

/* USER CODE BEGIN Prototypes */

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __USART_H__ */

