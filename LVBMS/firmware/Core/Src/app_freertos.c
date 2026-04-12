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
#include "longhorn/rtos/led.h"
#include "longhorn/rtos/logger.h"
#include "longhorn/rtos/usb.h"
#include "longhorn/usb_base.h"
#include "longhorn/rtos/dfu.h"
#include "tim.h"
#include "usbd_cdc_if.h"
#include "usart.h"
#include "adc.h"
#include "spi.h"
#include "gpio.h"
#include "adBms6830Data.h"
#include "adBms6830GenericType.h"
#include "adBms6830ParseCreate.h"
#include "adBms_Application.h"
#include "serialPrintResult.h"
#include "lvbms_bluetooth.h"
#include "lvbms_temperature_current.h"
#include "lvbms_fsm.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define TOTAL_IC 1
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
static cell_asic IC[TOTAL_IC];

osThreadId_t fsmTaskHandle;
const osThreadAttr_t fsmTask_attributes = {
  .name = "fsmTask",
  .priority = (osPriority_t) osPriorityAboveNormal,
  .stack_size = 512 * 4
};

osThreadId_t bleTaskHandle;
const osThreadAttr_t bleTask_attributes = {
  .name = "bleTask",
  .priority = (osPriority_t) osPriorityLow,
  .stack_size = 512 * 4
};
/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 256 * 4
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void StartFSMTask(void *argument);
void StartBLETask(void *argument);
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
    fsmTaskHandle = osThreadNew(StartFSMTask, NULL, &fsmTask_attributes);
    bleTaskHandle = osThreadNew(StartBLETask, NULL, &bleTask_attributes);

    rainbow_led_t led = {
        .ccr1 = &TIM2->CCR1,
        .ccr2 = &TIM2->CCR2,
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

  if (init_logging(CDC_Transmit_FS) == -1) {
    // If USB logging fails, stop LED thread so we notice
    // osThreadTerminate(ledHandle);
  }

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

  /* Infinite loop */

  for (;;) {
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
void StartFSMTask(void *argument) {
  /* USER CODE BEGIN StartFSMTask */
  bms_task_init(IC, TOTAL_IC);
  TickType_t xLastWakeTime = xTaskGetTickCount();

  for(;;)
  {
    printf("State: %d  Faults: 0x%02lX\r\n", bms_get_state(), bms_get_faults());
    bms_fsm_run();
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(BMS_TASK_PERIOD_MS));
  }
}

void StartBLETask(void *argument) {
  /* USER CODE BEGIN StartBLETask */
  RN4781_Init(&huart3);

  char buffer[100];

  for(;;)
  {
    /* 2. Grab the live system data */
    BmsState_t state = bms_get_state();
    uint32_t faults = bms_get_faults();

    /* 3. Format into a simple string 
        State: %d    -> The FSM integer
        Faults: 0x%X -> The hex representation of your fault bits */
    snprintf(buffer, sizeof(buffer), "ST: %d | FLT: 0x%02X\r\n", (int)state, (unsigned int)faults);

    /* 4. Send to the Bluetooth module */
    BLE_Send(buffer);

    /* 5. Update every 2 seconds for the test */
    osDelay(2000);
  }
}
/* USER CODE END Application */

