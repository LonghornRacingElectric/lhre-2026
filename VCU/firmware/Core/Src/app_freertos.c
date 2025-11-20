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
#include "rtos/dfu.h"
#include "rtos/led.h"
#include "rtos/logger.h"
#include "rtos/usb.h"
#include "tim.h"
#include "usb_base.h"
#include "usbd_cdc_if.h"


extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern ADC_HandleTypeDef hadc3;

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
osThreadId_t defaultTaskHandle;
osThreadId_t defaultTask2Handle;
osThreadId_t ledHandle;
osThreadId_t adcTaskHandle;

const osThreadAttr_t defaultTask_attributes = {
    .name = "defaultTask",
    .priority = (osPriority_t)osPriorityLow,
    .stack_size = 128 + 1024
};

const osThreadAttr_t defaultTask2_attributes = {
    .name = "defaultTask2",
    .priority = (osPriority_t)osPriorityLow,
    .stack_size = 128 + 1024
};

const osThreadAttr_t adcTask_attributes = {
    .name = "ADC_Task",
    .priority = (osPriority_t)osPriorityNormal,
    .stack_size = 128 + 512
};
/* USER CODE END Variables */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void StartDefaultTask(void* argument);
void StartDefaultTask2(void* argument);
void StartADCTask(void* argument);
/* USER CODE END FunctionPrototypes */

void MX_FREERTOS_Init(void) {
    /* USER CODE BEGIN Init */

    /* USER CODE END Init */

    /* Create the thread(s) */
    defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

    /* USER CODE BEGIN RTOS_THREADS */

    // LED setup
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
    ledHandle = led_start_thread();

    defaultTask2Handle = osThreadNew(StartDefaultTask2, NULL, &defaultTask2_attributes);

    // ADC Task
    adcTaskHandle = osThreadNew(StartADCTask, NULL, &adcTask_attributes);

    /* USER CODE END RTOS_THREADS */
}

/* USER CODE BEGIN StartDefaultTask */
void StartDefaultTask(void* argument) {
    MX_USB_Device_Init();

    if (init_logging(CDC_Transmit_FS) == -1) {
        osThreadTerminate(ledHandle);
    }

    dfu_config dfu = {
        .delay_fn = (Delay_fn)osDelay,
        .gpiox = GPIOB,
        .pin = GPIO_PIN_7,
        .pin_set_fn = (PinSet_fn)HAL_GPIO_WritePin,
        .reset_fn = (SystemReset_fn)HAL_NVIC_SystemReset,
    };

    init_dfu(dfu);
    dfu_start_thread();

    for (;;) {
        // ts_printf("Hello World! OS Tick: %d", osKernelGetTickCount());
        osDelay(pdMS_TO_TICKS(2905));
    }
}
/* USER CODE END StartDefaultTask */

void StartDefaultTask2(void* argument) {
    for (;;) {
        // ts_printf("2! Code Running from the VCU, OS Tick: %d", osKernelGetTickCount());
        osDelay(pdMS_TO_TICKS(1000));
    }
}

/* USER CODE BEGIN ADC_Task */
void StartADCTask(void* argument) {
    uint32_t adc1_val = 0;
    uint32_t adc2_val = 0;
    uint32_t adc3_val = 0;

    for (;;) {
        // Start and poll ADC1 PA3 - Steering angle
        HAL_ADC_Start(&hadc1);
        if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
            adc1_val = HAL_ADC_GetValue(&hadc1);
        }
        HAL_ADC_Stop(&hadc1);

        // Start and poll ADC2 PA6 - BSPD BSE
        HAL_ADC_Start(&hadc2);
        if (HAL_ADC_PollForConversion(&hadc2, 10) == HAL_OK) {
            adc2_val = HAL_ADC_GetValue(&hadc2);
        }
        HAL_ADC_Stop(&hadc2);

        // Start and poll ADC3 PD13 - APPS
        HAL_ADC_Start(&hadc3);
        if (HAL_ADC_PollForConversion(&hadc3, 10) == HAL_OK) {
            adc3_val = HAL_ADC_GetValue(&hadc3);
        }
        HAL_ADC_Stop(&hadc3);

        // Print values
        ts_printf("ADC1: %lu  ADC2: %lu  ADC3: %lu", adc1_val, adc2_val, adc3_val);

        osDelay(pdMS_TO_TICKS(500));
    }
}
/* USER CODE END ADC_Task */
