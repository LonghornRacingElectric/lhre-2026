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
  .priority = (osPriority_t) osPriorityHigh,
  .stack_size = 1024 * 4
};
/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 1024 * 4
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void StartFSMTask(void *argument);
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

  RN4781_Init(&huart3);
  char buf[200];

  const char *state_names[] = {
        "INIT",
        "IDLE",
        "MEASURING",
        "BALANCING",
        "FAULT",
        "SHUTDOWN"
    };

    const char *fault_names[] = {
        "NONE",
        "OV",
        "UV",
        "",
        "OT",
        "",
        "",
        "",
        "OC"
    };

  /* Infinite loop */

  for (;;) {
    snprintf(buf, sizeof(buf),
            "State: %s Faults: %s\r\n"
            "C1:%.2f C2:%.2f C3:%.2f C4:%.2f\r\n"
            "C5:%.2f C6:%.2f C7:%.2f\r\n"
            "T1:%.1f T2:%.1f T3:%.1f\r\n"
            "Current:%.2f\r\n",
            state_names[bms_get_state()], fault_names[bms_get_faults()],
            bms_get_cell_voltage(0), bms_get_cell_voltage(1),
            bms_get_cell_voltage(2), bms_get_cell_voltage(3),
            bms_get_cell_voltage(4), bms_get_cell_voltage(5),
            bms_get_cell_voltage(6),
            bms_get_temperature(0), bms_get_temperature(1),
            bms_get_temperature(2),
            bms_get_current()
    );
    BLE_Send(buf);
    osDelay(2000);
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
    //printf("State: %d  Faults: 0x%02lX\r\n", bms_get_state(), bms_get_faults());
    bms_fsm_run();
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(BMS_TASK_PERIOD_MS));
  }
}
/* USER CODE END Application */

