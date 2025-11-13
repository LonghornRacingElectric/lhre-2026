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
#include "longhorn/rtos/led.h"
#include "longhorn/rtos/logger.h"
#include "longhorn/rtos/usb.h"
#include "longhorn/usb_base.h"
#include "tim.h"
#include "usbd_cdc_if.h"
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
osThreadId_t ledHandle;
const osThreadAttr_t defaultTask_attributes = {
    .name = "defaultTask",
    .priority = (osPriority_t)osPriorityLow,
    .stack_size = 128 + 2048};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void* argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

void StartDefaultTask2(void* argument) {
    /* init code for USB_Device */
    /* USER CODE BEGIN StartDefaultTask */

    /* Infinite loop */

    for (;;) {
        ts_printf2("2! Code Running from the VCU, current OS Tick: %d",
                   osKernelGetTickCount());
        osDelay(pdMS_TO_TICKS(1000));
    }
    /* USER CODE END StartDefaultTask */
}

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

    rainbow_led_t led = {
        .ccr1 = &TIM2->CCR1,
        .ccr2 = &TIM2->CCR2,
        .ccr3 = &TIM2->CCR3,
        .channel1 = TIM_CHANNEL_1,
        .channel2 = TIM_CHANNEL_2,
        .channel3 = TIM_CHANNEL_3,
        .pwm_start = HAL_TIM_PWM_Start,
        .timer_handle = &htim2,
    };

    led_init(&led);
    ledHandle = led_start_thread();

    osThreadNew(StartDefaultTask2, NULL, NULL);

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
void StartDefaultTask(void* argument) {
    /* init code for USB_Device */
    MX_USB_Device_Init();
    /* USER CODE BEGIN StartDefaultTask */

    init_usb(CDC_Transmit_FS);
    // if (init_logging(CDC_Transmit_FS) == -1) {
    //     osThreadTerminate(ledHandle);
    // };

    /* Infinite loop */

    for (;;) {
        ts_printf2(
            "Hello World! Code Running from the VCU, current OS Tick: %d",
            osKernelGetTickCount());
        osDelay(pdMS_TO_TICKS(2905));
    }
    /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */
