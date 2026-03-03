/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name          : app_freertos.c
 * Description        : Code for freertos applications
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "main.h"
#include "task.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <spi.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
    .name = "defaultTask",
    .priority = (osPriority_t)osPriorityNormal,
    .stack_size = 128 * 4};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
 * @brief  FreeRTOS initialization
 * @param  None
 * @retval None
 */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle =
      osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */

  /* USER CODE END RTOS_EVENTS */
}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
 * @brief  Function implementing the defaultTask thread.
 * @param  argument: Not used
 * @retval None
 */

void ST7789_WriteCommand(uint8_t cmd) {
  HAL_GPIO_WritePin(LCD_NCS_GPIO_Port, LCD_NCS_Pin,
                    GPIO_PIN_RESET); // CS Low to begin
  HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin,
                    GPIO_PIN_RESET); // DC Low for Command
  HAL_SPI_Transmit(&hspi2, &cmd, 1, HAL_MAX_DELAY);
  HAL_GPIO_WritePin(LCD_NCS_GPIO_Port, LCD_NCS_Pin,
                    GPIO_PIN_SET); // CS High to end
}

void ST7789_WriteData(uint8_t data) {
  HAL_GPIO_WritePin(LCD_NCS_GPIO_Port, LCD_NCS_Pin,
                    GPIO_PIN_RESET); // CS Low to begin
  HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin,
                    GPIO_PIN_SET); // DC High for Data
  HAL_SPI_Transmit(&hspi2, &data, 1, HAL_MAX_DELAY);
  HAL_GPIO_WritePin(LCD_NCS_GPIO_Port, LCD_NCS_Pin,
                    GPIO_PIN_SET); // CS High to end
}

void ST7789_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
  ST7789_WriteCommand(0x2A); // Column Address Set
  ST7789_WriteData(x0 >> 8);
  ST7789_WriteData(x0 & 0xFF);
  ST7789_WriteData(x1 >> 8);
  ST7789_WriteData(x1 & 0xFF);

  ST7789_WriteCommand(0x2B); // Row Address Set
  ST7789_WriteData(y0 >> 8);
  ST7789_WriteData(y0 & 0xFF);
  ST7789_WriteData(y1 >> 8);
  ST7789_WriteData(y1 & 0xFF);

  ST7789_WriteCommand(0x2C); // Memory Write
}

void ST7789_TestPattern(void) {
  // Clear screen to Black (0x0000)
  ST7789_SetWindow(0, 0, 239, 239);
  HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET);

  uint8_t black[2] = {0, 0};
  uint8_t white[2] = {0xFF, 0xFF};

  for (int y = 0; y < 240; y++) {
    for (int x = 0; x < 240; x++) {
      // Create a grid: white pixel every 20 pixels
      if (x % 20 == 0 || y % 20 == 0) {
        HAL_SPI_Transmit(&hspi2, white, 2, HAL_MAX_DELAY);
      } else {
        HAL_SPI_Transmit(&hspi2, black, 2, HAL_MAX_DELAY);
      }
    }
  }
}
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument) {
  /* USER CODE BEGIN StartDefaultTask */

  // 1. HARDWARE RESET
  HAL_GPIO_WritePin(LCD_RES_GPIO_Port, LCD_RES_Pin, GPIO_PIN_RESET);
  osDelay(50);
  HAL_GPIO_WritePin(LCD_RES_GPIO_Port, LCD_RES_Pin, GPIO_PIN_SET);
  osDelay(50);

  // 2. ACTIVATE CHIP SELECT
  // Your schematic shows LCD_CS on pin 9 of J2.
  // Ensure this GPIO is initialized in CubeMX.
  HAL_GPIO_WritePin(LCD_NCS_GPIO_Port, LCD_NCS_Pin, GPIO_PIN_RESET);

  // 3. MINIMAL INIT SEQUENCE
  ST7789_WriteCommand(0x01); // Software Reset
  osDelay(150);
  ST7789_WriteCommand(0x11); // Sleep Out
  osDelay(120);

  ST7789_WriteCommand(0x3A); // Interface Pixel Format
  ST7789_WriteData(0x05);    // 16-bit/pixel (RGB565)

  ST7789_WriteCommand(0x36); // MADCTL: Memory Data Access Control
  ST7789_WriteData(
      0x00); // 0x00 is default. Use 0x70, 0xC0 etc., to rotate screen

  ST7789_WriteCommand(
      0x21); // Display Inversion ON (Crucial for most ST7789 IPS panels)

  ST7789_WriteCommand(0x29); // Display ON
  osDelay(10);

  // 4. DRAW TEST LINES (Red)

  for (;;) {
    HAL_GPIO_TogglePin(LEDG_GPIO_Port, LEDG_Pin);

    osDelay(500);

    // 4. DRAW TEST LINES (Red)
    ST7789_SetWindow(0, 0, 239, 239);

    // Turn on backlight
    HAL_GPIO_WritePin(BACKLIGHT_EN_GPIO_Port, BACKLIGHT_EN_Pin, GPIO_PIN_SET);

    uint16_t red = 0xF800; // RGB565 Red
    uint8_t data[2] = {red >> 8, red & 0xFF};

    // Prepare for bulk data transfer
    HAL_GPIO_WritePin(LCD_NCS_GPIO_Port, LCD_NCS_Pin,
                      GPIO_PIN_RESET); // Bulk CS Low
    HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin,
                      GPIO_PIN_SET); // DC High for Data

    for (int i = 0; i < 240 * 240; i++) {
      HAL_SPI_Transmit(&hspi2, data, 2, HAL_MAX_DELAY);
    }

    HAL_GPIO_WritePin(LCD_NCS_GPIO_Port, LCD_NCS_Pin,
                      GPIO_PIN_SET); // Bulk CS High
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */
