/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name          : app_freertos.c
 * Description        : Code for freertos applications
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

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "main.h"
#include "task.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "cooling.h"
#include "lights.h"
#include "longhorn/rtos/dfu.h"
#include "longhorn/rtos/led.h"
#include "longhorn/rtos/logger.h"
#include "ota/ota_flash.h"
#include "pdu_can.h"
#include "tim.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"
#include <stm32g4xx_hal_gpio.h>

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
dfu_config dfu = {
    .delay_fn = (Delay_fn)osDelay,
    .gpiox = GPIOB,
    .pin = GPIO_PIN_7,
    .pin_set_fn = (PinSet_fn)HAL_GPIO_WritePin,
    .reset_fn = (SystemReset_fn)HAL_NVIC_SystemReset,
    .set_bank1_fn = (SetBank1_fn)ota_set_bank1,
};

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
    .name = "defaultTask",
    .priority = (osPriority_t)osPriorityNormal,
    .stack_size = 1024 * 4};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
osThreadId_t ledHandle;

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
  rainbow_led_t led = {
      .ccr2 = &TIM8->CCR1,
      .ccr1 = &TIM8->CCR2,
      .ccr3 = &TIM8->CCR3,
      .channel1 = TIM_CHANNEL_1,
      .channel2 = TIM_CHANNEL_2,
      .channel3 = TIM_CHANNEL_3,
      .pwm_start = (HAL_PWM_Start_Fn)HAL_TIM_PWM_Start,
      .timer_handle = &htim8,
  };

  led_init(&led);
  ledHandle = led_start_thread();

  // Initialize CAN
  pdu_can_init();

  // Initialize Light subsystem
  lights_init();

  // Initialize Cooling subsystem
  cooling_init();

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
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument) {
  /* init code for USB_Device */
  MX_USB_Device_Init();
  /* USER CODE BEGIN StartDefaultTask */

  // setup USB and Logging
  if (init_logging(CDC_Transmit_FS) == -1) {
    osThreadTerminate(ledHandle);
  }

  init_dfu(dfu);
  dfu_start_thread();

  // Enable board power
  HAL_GPIO_WritePin(SW_BOARDS_GPIO_Port, SW_BOARDS_Pin, GPIO_PIN_SET);

  // Enable cooling stuff
  HAL_GPIO_WritePin(SW_FANS_GPIO_Port, SW_FANS_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(SW_BATT_FANS1_GPIO_Port, SW_BATT_FANS1_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(SW_BATT_FANS2_GPIO_Port, SW_BATT_FANS2_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(SW_PUMPS_GPIO_Port, SW_PUMPS_Pin, GPIO_PIN_RESET);

  // Enable shutdown
  HAL_GPIO_WritePin(EN_SDWN_GPIO_Port, EN_SDWN_Pin, GPIO_PIN_SET);

  /* Infinite loop */
  for (;;) {
    osDelay(200);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */
