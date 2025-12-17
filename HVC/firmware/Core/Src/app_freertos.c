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
#include "longhorn/can_base.h"
#include "tim.h"
#include "usbd_cdc_if.h"
#include <stdint.h>

/* HVC Application Modules */
#include "hvc_state_machine.h"
#include "hvc_contactors.h"
#include "hvc_bms.h"
#include "can.h"
#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "tim.h"
#include "fdcan.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
extern SPI_HandleTypeDef hspi4;
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
osThreadId_t bmsTaskHandle;
const osThreadAttr_t bmsTask_attributes = {
  .name = "bms_Task",
  .priority = (osPriority_t) osPriorityNormal,  // Same as other tasks
  .stack_size = 256 * 8 * 2
};
osThreadId_t stateMachineTaskHandle;
const osThreadAttr_t stateMachineTask_attributes = {
  .name = "stateMachine",
  .priority = (osPriority_t) osPriorityHigh,
  .stack_size = 128 * 8 * 2
};

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 2048 * 4
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
  bmsTaskHandle = osThreadNew(StartBmsTask, NULL, &bmsTask_attributes);
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
  
  // CDC_Transmit_FS((uint8_t*)msg, strlen(msg));

  // usb_init(CDC_Transmit_FS)
  init_logging(CDC_Transmit_FS);

  const char* msg = "\r\n=== HVC Firmware Starting ===\r\n";
  log_printf(LOG_WARNING, msg);

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
  // can_init(&can_config);
  
  msg = "DFU initialized\r\n";
  log_printf(LOG_INFO, msg);
  
  msg = "System running\r\n";
  log_printf(LOG_INFO, msg);
  
  
// static can_config_t can_config = {
//         .init_fn = (CAN_Init_fn)HAL_FDCAN_Init,
//         .start_fn = (CAN_Start_fn)HAL_FDCAN_Start,
//         .noti_fn = (CAN_ActivateNotifications_fn)HAL_FDCAN_ActivateNotification,
//         .stop_fn = (CAN_Stop_fn)HAL_FDCAN_Stop,
//         .add_to_queue_fn = (CAN_AddToQ_fn)HAL_FDCAN_AddMessageToTxFifoQ,
//         .get_rx_message_fn = (CAN_GetRxMessage_fn)HAL_FDCAN_GetRxMessage,
//         .tick_fn = (Tick_fn)osKernelGetTickCount,
//         .add_filter_fn = (CAN_AddFilter_fn)HAL_FDCAN_ConfigFilter,
//         .malloc_fn = (Malloc_fn)pvPortMalloc,
//         .free_fn = (Free_fn)vPortFree,
//     };

//     can_init(&can_config);

//     static can_interface_t can_interface = {
//         .handle = &hfdcan1,
//     };

//     can_register_interface(&can_interface);

//     static msg_vcu_current_sense_t msg_content;
//     can_message_t* msg_can = can_get_message_handle(
//         &msg_content, VCU_CURRENT_SENSE_ID, VCU_CURRENT_SENSE_FREQ,
//         VCU_CURRENT_SENSE_DLC, pack_vcu_current_sense);
//     can_register_send_packet(&can_interface, msg_can);

// static msg_vcu_fuses_t rx_fuses;
//     can_receive_message_t* rx_msg = can_get_receive_message_handle(
//         &rx_fuses, VCU_FUSES_ID, unpack_vcu_fuses);
//     can_register_receive_packet(&can_interface, rx_msg);
  /* Infinite loop */
  for(;;)
  {
    osDelay(1000);
    // can_service(&can_interface);
    // log_printf(LOG_WARNING, "Message Received: %f", rx_fuses.vcu_fuses_1);
    // msg_content.lv_boards_current = 1.0f;
    // can_func();
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
  log_printf(LOG_INFO, msg);
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



/* USER CODE END Application */

