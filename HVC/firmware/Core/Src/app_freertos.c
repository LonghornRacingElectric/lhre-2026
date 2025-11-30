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
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "longhorn/rtos/dfu.h"
#include "longhorn/rtos/led.h"
#include "longhorn/rtos/logger.h"
#include "longhorn/rtos/usb.h"
#include "longhorn/usb_base.h"
#include "tim.h"
#include "usbd_cdc_if.h"

/* HVC Application Modules */
#include "hvc_state_machine.h"
#include "hvc_contactors.h"
#include "hvc_bms.h"
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

/* Definitions for stateMachineTask */
osThreadId_t stateMachineTaskHandle;
const osThreadAttr_t stateMachineTask_attributes = {
  .name = "stateMachine",
  .priority = (osPriority_t) osPriorityHigh,
  .stack_size = 256 * 4
};

/* Definitions for bmsTask */
osThreadId_t bmsTaskHandle;
const osThreadAttr_t bmsTask_attributes = {
  .name = "bmsTask",
  .priority = (osPriority_t) osPriorityAboveNormal,
  .stack_size = 512 * 4
};

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 128 * 4
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void StartStateMachineTask(void *argument);
void StartBmsTask(void *argument);
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
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  
  /* HVC State Machine Task - 10Hz update rate */
  stateMachineTaskHandle = osThreadNew(StartStateMachineTask, NULL, &stateMachineTask_attributes);
  
  /* HVC BMS Task - 5Hz update rate (200ms period) */
  bmsTaskHandle = osThreadNew(StartBmsTask, NULL, &bmsTask_attributes);
  
  // Check if BMS task creation failed
  if (bmsTaskHandle == NULL) {
    // Task creation failed - likely out of heap memory
    // This will be checked in default task
  }
  
  /* Rainbow LED for visual feedback */
  rainbow_led_t led = {
    .ccr2 = &TIM2->CCR1,
    .ccr1 = &TIM2->CCR2,
    .ccr3 = &TIM2->CCR3,
    .channel1 = TIM_CHANNEL_1,
    .channel2 = TIM_CHANNEL_2,
    .channel3 = TIM_CHANNEL_3,
    .pwm_start = (HAL_PWM_Start_Fn)HAL_TIM_PWM_Start,
    .timer_handle = &htim2,
  };

  led_init(&led);
  led_start_thread();
  
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
void StartDefaultTask(void *argument)
{
  /* init code for USB_Device */
  MX_USB_Device_Init();
  /* USER CODE BEGIN StartDefaultTask */
  
  // Wait for USB to enumerate
  osDelay(3000);
  
  const char* msg = "\r\n=== HVC Firmware Starting ===\r\n";
  CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
  osDelay(100);
  
  // Initialize DFU
  dfu_config dfu = {
    .delay_fn = (Delay_fn)osDelay,
    .gpiox = GPIOB,
    .pin = GPIO_PIN_7,
    .pin_set_fn = (PinSet_fn)HAL_GPIO_WritePin,
    .reset_fn = (SystemReset_fn)HAL_NVIC_SystemReset,
  };

  init_dfu(dfu);
  dfu_start_thread();
  
  msg = "DFU initialized\r\n";
  CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
  osDelay(100);
  
  // Check BMS task creation
  if (bmsTaskHandle == NULL) {
    msg = "ERROR: BMS task creation failed!\r\n";
    CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
    osDelay(100);
  } else {
    msg = "BMS task created successfully\r\n";
    CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
    osDelay(100);
  }
  
  msg = "System running - BMS task disabled for debugging\r\n";
  CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
  osDelay(100);
  
  /* Infinite loop */
  for(;;)
  {
    osDelay(1000);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/**
 * @brief State Machine Task
 * @details Runs at 10Hz to manage HVC states, precharge, and contactor control
 * @param argument Not used
 */
void StartStateMachineTask(void *argument)
{
  // Wait for USB
  osDelay(3500);
  
  const char* msg = "State Machine initialized\r\n";
  CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
  osDelay(200);
  
  // Initialize state machine and contactors
  state_machine_init();
  contactors_init();
  
  // Task loop - 10Hz update rate (100ms period)
  const uint32_t task_period_ms = 100;
  
  for(;;)
  {
    // Update state machine
    update_state_machine();
    
    osDelay(task_period_ms);
  }
}

/**
 * @brief BMS Task
 * @details Runs at 5Hz to read cell voltages and temperatures from ADBMS6830 chips
 * @param argument Not used
 */
void StartBmsTask(void *argument)
{
  // Wait for USB - BMS starts after state machine
  osDelay(4500);
  
  const char* msg = "BMS Task started\r\n";
  CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
  osDelay(200);
  
  // Initialize BMS
  bms_init();
  
  msg = "BMS initialized - ready for development\r\n";
  CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
  osDelay(200);
  
  // Task loop - 5Hz update rate (200ms period)
  const uint32_t task_period_ms = 200;
  uint32_t loop_count = 0;
  char buffer[128];
  
  for(;;)
  {
    // Update BMS readings
    bms_update();
    
    // Heartbeat every 5 seconds while we develop the driver
    if (loop_count % 25 == 0) {
      snprintf(buffer, sizeof(buffer), "BMS heartbeat: %lu\r\n", loop_count / 25);
      CDC_Transmit_FS((uint8_t*)buffer, strlen(buffer));
    }
    
    loop_count++;
    osDelay(task_period_ms);
  }
}

/* USER CODE END Application */

