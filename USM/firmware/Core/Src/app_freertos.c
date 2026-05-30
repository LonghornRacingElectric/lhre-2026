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

#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "tim.h"
#include "usbd_cdc_if.h"

#include "longhorn/rtos/dfu.h"
#include "longhorn/rtos/led.h"
#include "longhorn/rtos/logger.h"
#include "longhorn/rtos/usb.h"
#include "longhorn/usb_base.h"
#include "ota/ota_flash.h"

#include "imu.h"
#include "spi.h"
#include "usm_can.h"
#include "wheel_speed.h"
#include <stdio.h>

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
static dfu_config dfu_conf = {
    .delay_fn = (Delay_fn)osDelay,
    .gpiox = GPIOB,
    .pin = GPIO_PIN_9,
    .pin_set_fn = (PinSet_fn)HAL_GPIO_WritePin,
    .reset_fn = (SystemReset_fn)HAL_NVIC_SystemReset,
    .set_bank1_fn = (SetBank1_fn)ota_set_bank1,
};

static rainbow_led_t led_conf = {
    .ccr2 = &TIM2->CCR1,
    .ccr1 = &TIM2->CCR2,
    .ccr3 = &TIM2->CCR3,
    .channel1 = TIM_CHANNEL_1,
    .channel2 = TIM_CHANNEL_2,
    .channel3 = TIM_CHANNEL_3,
    .pwm_start = (HAL_PWM_Start_Fn)HAL_TIM_PWM_Start,
    .timer_handle = &htim2,
};

osThreadId_t wheelSpeedTaskHandle;
const osThreadAttr_t wheelSpeedTask_attributes = {
    .name = "wheelSpeedTask",
    .priority = (osPriority_t)osPriorityNormal,
    .stack_size = 512 * 8};
/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
    .name = "defaultTask",
    .priority = (osPriority_t)osPriorityNormal,
    .stack_size = 512 * 4};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
osThreadId_t ledHandle;

void StartWheelSpeedTask(void *argument);
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
  led_init(&led_conf);
  ledHandle = led_start_thread();

  wheelSpeedTaskHandle =
      osThreadNew(StartWheelSpeedTask, NULL, &wheelSpeedTask_attributes);
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
  if (init_logging(CDC_Transmit_FS) == -1) {
    osThreadTerminate(ledHandle);
  }

  init_dfu(dfu_conf);
  dfu_start_thread();

  usm_can_init();

  /* Infinite loop */
  for (;;) {
    usm_can_debug();
    osDelay(1000);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
void StartWheelSpeedTask(void *argument) {
  osDelay(2000); // wait for USB CDC to enumerate
  WheelSpeed_Init(&hspi2);
  IMU_Init(&hspi2);
  imu_data_t imu = {0};
  for (;;) {
    WheelSpeed_Update();

    float speed = WheelSpeed_GetSpeed();
    usm_can_update_wheel_speed(speed);

    IMU_Read(&imu);
    usm_can_update_accel(imu.accel_x, imu.accel_y, imu.accel_z);

    // log_printf(LOG_INFO, "RPM:%.1f MPH:%.2f Ax:%.2f Ay:%.2f Az:%.2f\r\n", rpm,
    //            mph, imu.accel_x, imu.accel_y, imu.accel_z);
    osDelay(1);
  }
}
/* USER CODE END Application */
