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
#include "LCDController.h"
#include "tim.h"
#include <cmsis_os2.h>
#include <screens/ui_Screen1.h>
#include <spi.h>
#include <stdio.h>
#include <stm32g4xx_hal_tim.h>
#include <ui.h>
#include <ui_helpers.h>

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

/* Mutex for protecting LVGL access from multiple tasks */
osMutexId_t lvgl_mutex;
const osMutexAttr_t lvgl_mutex_attr = {
    "lvgl_mutex",                          // human readable name
    osMutexRecursive | osMutexPrioInherit, // attr_bits
    NULL,                                  // memory
    0                                      // size
};

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
    .name = "defaultTask",
    .priority = (osPriority_t)osPriorityNormal,
    .stack_size = 512 * 4};

const osThreadAttr_t lvgl_tick_attributes = {.name = "lvgl_tick",
                                             .priority =
                                                 (osPriority_t)osPriorityNormal,
                                             .stack_size = 256 * 4};

const osThreadAttr_t lvgl_timer_attributes = {
    .name = "lvgl_timer",
    .priority = (osPriority_t)osPriorityNormal,
    .stack_size = 512 * 4};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
/* LVGL timer for tasks. */
void LVGLTimer(void *argument) {
  for (;;) {
    osMutexAcquire(lvgl_mutex, osWaitForever);
    lv_timer_handler();
    osMutexRelease(lvgl_mutex);
    osDelay(20);
  }
}
/* LVGL tick source */
void LVGLTick(void *argument) {
  for (;;) {
    lv_tick_inc(10);
    osDelay(10);
  }
}

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
  lvgl_mutex = osMutexNew(&lvgl_mutex_attr);
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
  osThreadNew(LVGLTick, NULL, &lvgl_tick_attributes);
  osThreadNew(LVGLTimer, NULL, &lvgl_timer_attributes);
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

/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument) {
  /* USER CODE BEGIN StartDefaultTask */

  /* Hardware reset the display */
  HAL_GPIO_WritePin(LCD_RES_GPIO_Port, LCD_RES_Pin, GPIO_PIN_RESET);
  osDelay(50);
  HAL_GPIO_WritePin(LCD_RES_GPIO_Port, LCD_RES_Pin, GPIO_PIN_SET);
  osDelay(50);
  HAL_GPIO_WritePin(BACKLIGHT_EN_GPIO_Port, BACKLIGHT_EN_Pin, GPIO_PIN_SET);

  /* Initialize LVGL and the display driver (must be done after RTOS starts,
   * because ST7789_Init uses osDelay) */
  lv_init();
  lv_port_disp_init();
  ui_init();

  lv_scr_load_anim(ui_Screen1, LV_SCR_LOAD_ANIM_FADE_IN, 500, 0, true);

  // lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x003a57),
  // LV_PART_MAIN);

  /*Create a spinner*/
  // lv_obj_t *spinner = lv_spinner_create(lv_scr_act(), 1000, 60);
  // lv_obj_set_size(spinner, 64, 64);
  // lv_obj_align(spinner, LV_ALIGN_BOTTOM_MID, 0, 0);

  char buffer[100];
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
  uint32_t diff = 0;
  float pct = 0.0f;
  for (;;) {
    uint32_t start = osKernelGetTickCount();
    sprintf(buffer, "Hello World %u", diff);
    lv_textarea_set_text(ui_TextArea1, buffer);
    lv_bar_set_value(ui_Bar2, (int32_t)(pct * 100), LV_ANIM_ON);
    osMutexRelease(lvgl_mutex);
    pct += 0.01f;
    if (pct > 1.0f) {
      pct = 0.0f;
    }
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, (uint32_t)(pct * 1000));
    osDelay(50);

    osMutexAcquire(lvgl_mutex, osWaitForever);
    diff = osKernelGetTickCount() - start;
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */
