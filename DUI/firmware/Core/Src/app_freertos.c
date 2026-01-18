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
#include "adc.h"
#include "fdcan.h"
#include "longhorn/rtos/dfu.h"
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
const osThreadAttr_t defaultTask_attributes = {
    .name = "defaultTask",
    .priority = (osPriority_t)osPriorityNormal,
    .stack_size = 128 + 1024};

uint32_t AD_RES = 0;

/* Private function prototypes
   -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
osThreadId_t led_handle;

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void* argument);

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
    led_handle = led_start_thread();

    /* USER CODE END RTOS_THREADS */

    /* USER CODE BEGIN RTOS_EVENTS */
    /* add events, ... */
    /* USER CODE END RTOS_EVENTS */
}

const osThreadAttr_t fakecantaskattributes = {
    .name = "fakecan",
    .priority = (osPriority_t)osPriorityNormal,
    .stack_size = 128 + 1024};

void fakeCANTask(void* argument) {
    // initialize can buses
    HAL_FDCAN_Init(&hfdcan2);
    HAL_FDCAN_Init(&hfdcan1);
    HAL_FDCAN_Start(&hfdcan1);
    HAL_FDCAN_Start(&hfdcan2);

    FDCAN_TxHeaderTypeDef tx_header;
    char fakedata[1] = {100};

    tx_header.DataLength = 1;
    tx_header.IdType = FDCAN_STANDARD_ID;
    tx_header.Identifier = 0x100;
    tx_header.TxFrameType = FDCAN_DATA_FRAME;
    tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    tx_header.BitRateSwitch = FDCAN_BRS_OFF;
    tx_header.FDFormat = FDCAN_CLASSIC_CAN;
    tx_header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    for (;;) {
        HAL_StatusTypeDef hal_status =
            HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &tx_header, &fakedata);

        if (hal_status) {
            // error occurred
            // log_printf(LOG_ERROR, "Sending FDCAN Data: %d, Bus Status: %d",
            //     fakedata[0], hal_status);
        } else {
            // log_printf(LOG_INFO, "Sending FDCAN Data: %d, Bus Status: %d",
            // fakedata[0],
            //     hal_status);
        }

        hal_status =
            HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan2, &tx_header, &fakedata);
        // log_printf(hal_status ? LOG_ERROR : LOG_SUCCESS,
        //     "Sending FDCAN2 Data: %d, Bus Status: %d", fakedata[0],
        //     hal_status);

        fakedata[0]++;

        osDelay(33);
    }
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

    if (init_logging(CDC_Transmit_FS) == -1) {
        osThreadTerminate(led_handle);
    }

    dfu_config dfu_conf = {
        .delay_fn = osDelay,
        .gpiox = GPIOB,
        .pin = GPIO_PIN_7,
        .pin_set_fn = HAL_GPIO_WritePin,
        .reset_fn = HAL_NVIC_SystemReset,
    };

    osThreadNew(fakeCANTask, NULL, &fakecantaskattributes);

    init_dfu(dfu_conf);
    dfu_start_thread();
    HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);

    /* Infinite loop */

    for (;;) {
        // log_printf(LOG_INFO, "Main thread! Code Running from the DUI",
        //     osKernelGetTickCount());
        log_printf(LOG_INFO, "The ADC data was %d", AD_RES);

        osDelay(pdMS_TO_TICKS(1000));

        HAL_ADC_Start_DMA(&hadc1, &AD_RES, 1);
    }
    /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */
