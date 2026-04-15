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
#include <stdint.h>

#include "FreeRTOS.h"
#include "adc.h"
#include "can.h"
#include "fdcan.h"
#include "longhorn/rtos/dfu.h"
#include "longhorn/rtos/led.h"
#include "longhorn/rtos/logger.h"
#include "longhorn/rtos/usb.h"
#include "longhorn/usb_base.h"
#include "tim.h"
#include "usbd_cdc_if.h"

/* HVC Application Modules */
#include "hvc_bms.h"
#include "hvc_contactors.h"
#include "hvc_faults.h"
#include "hvc_state_machine.h"
#include "hvc_vct_sense.h"
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
    .priority = (osPriority_t)osPriorityHigh2,
    .stack_size = 256 * 8 * 2 * 2};
osThreadId_t stateMachineTaskHandle;
const osThreadAttr_t stateMachineTask_attributes = {
    .name = "stateMachine",
    .priority = (osPriority_t)osPriorityHigh,
    .stack_size = 128 * 8 * 2};

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
  stateMachineTaskHandle =
      osThreadNew(StartStateMachineTask, NULL, &stateMachineTask_attributes);
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

  const char *msg = "\r\n=== HVC Firmware Starting ===\r\n";
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

  osThreadAttr_t fakeTask_attributes = {
      .name = "FakeTask",
      .priority = (osPriority_t)osPriorityLow,
      .stack_size = 128 * 8 * 2,
  };

  // osThreadNew(FakeTask, NULL, &fakeTask_attributes);
  //  can_init(&can_config);

  msg = "DFU initialized\r\n";
  log_printf(LOG_INFO, msg);

  msg = "System running\r\n";
  log_printf(LOG_INFO, msg);

  // Initialize CAN (contactor status, data, etc.)
  hvc_can_init();

  /* Infinite loop */
  for (;;) {
    // log_printf(LOG_INFO, "Tractive Voltage: %.2f V, Raw: %d\r\n",
    // get_tractive_voltage(), hvc_adc_read_voltage_sense_raw());
    // log_printf(LOG_INFO, "Is Shutdown Closed: %d\r\n",
    // is_shutdown_closed());
    osDelay(200);
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
void StartStateMachineTask(void *argument) {
  // Wait for USB
  osDelay(3500);
  const char *msg = "State Machine initialized\r\n";
  log_printf(LOG_INFO, msg);
  osDelay(200);

  // Initialize state machine and contactors
  state_machine_init();
  contactors_init();
  faults_init();

  // Task loop - 10Hz update rate (100ms period)
  const uint32_t task_period_ms = 100;
  uint32_t latched_fault_vector = 0;

  for (;;) {
    // Update state machine
    uint32_t current_fault_vector = get_faults();
    latched_fault_vector |= current_fault_vector;
    set_bms_fault_pin(current_fault_vector != 0);
    bool any_faults = (latched_fault_vector != 0) ||
                      (osKernelGetTickCount() <
                       5000); // decreasing may introduce race condition

    update_state_machine(any_faults);
    hvc_update_contactor_status();

    log_printf(LOG_INFO,
               "Pack: %.2f, Tractive: %.2f V, Raw: %.2f, "
               "Faults: 0x%08lX, MinV: %.3f, MaxV: %.3f, MinT: %.1f, MaxT: %.1f, "
               "State: %d, Shutdown: %d, BMS Responsive: %d\n",
               get_pack_voltage(), get_tractive_voltage(),
               hvc_adc_read_voltage_sense_v(),
               latched_fault_vector, bms_get_min_voltage(), bms_get_max_voltage(),
               bms_get_min_temp(), bms_get_max_temp(), get_current_state(),
               is_shutdown_closed(), bms_get_num_responsive_ics());
    osDelay(task_period_ms);
  }
}

/* USER CODE END Application */

