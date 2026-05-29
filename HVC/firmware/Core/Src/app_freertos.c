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
#include "gpio.h"
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
    .stack_size = 8192};
osThreadId_t stateMachineTaskHandle;
const osThreadAttr_t stateMachineTask_attributes = {
    .name = "stateMachine",
    .priority = (osPriority_t)osPriorityHigh,
    .stack_size = 4096};

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
    bool bms_indicator_error =
        (current_fault_vector &
         (FAULT_BMS_COMMS | FAULT_BMS_OVERVOLTAGE |
          FAULT_BMS_UNDERVOLTAGE | FAULT_BMS_OVERTEMP)) != 0;
    hvc_set_indicator_status(bms_indicator_error,
                             hvc_gpio_is_imd_error_active(),
                             hvc_gpio_is_shutdown_leg1_closed(),
                             hvc_gpio_is_shutdown_leg2_closed(),
                             hvc_gpio_is_shutdown_leg3_closed(),
                             hvc_gpio_is_shutdown_leg4_closed());
    bool any_faults = (latched_fault_vector != 0) ||
                      (osKernelGetTickCount() < 5000);

    update_state_machine(any_faults);
    hvc_update_contactor_status();

    float pack_v = bms_get_pack_voltage();
    float tractive_v = get_tractive_voltage();
    float tractive_a = get_tractive_current();
    float raw_vsense_v = hvc_adc_read_voltage_sense_v();
    float min_v = bms_get_min_voltage();
    float max_v = bms_get_max_voltage();
    float delta_mv = (max_v - min_v) * 1000.0f;
    float min_temp_c = bms_get_min_temp();
    float max_temp_c = bms_get_max_temp();
    float max_die_temp_c = bms_get_max_die_temp();
    uint8_t responsive_ics = bms_get_num_responsive_ics();
    uint8_t balance_count = bms_get_balance_count();
    bool bms_disc = bms_check_disconnection();
    bool bms_uv = bms_check_undervoltage();
    bool bms_ov = bms_check_overvoltage();
    bool bms_ot = bms_check_overtemp();
    
    log_printf(LOG_INFO,
               "Pack: %.2f V, Faults[live:0x%08lX latched:0x%08lX], "
               "Cell[min:%.3f max:%.3f dV:%.1f mV], Temp[min:%.1f max:%.1f], "
               "DieTemp[max:%.1f], "
               "BMS[resp:%u disc:%u uv:%u ov:%u ot:%u], "
               "State:%d Shutdown:%d BalCnt:%u\n",
               pack_v,
               current_fault_vector, latched_fault_vector,
               min_v, max_v, delta_mv, min_temp_c, max_temp_c,
               max_die_temp_c,
               responsive_ics, bms_disc, bms_uv, bms_ov, bms_ot,
               get_current_state(), is_shutdown_closed(), balance_count);
    osDelay(task_period_ms);

    if(get_current_state() != HVC_STATE_ENERGIZED) {
      bms_print_all_cells();
    }
  }
}

/* USER CODE END Application */
