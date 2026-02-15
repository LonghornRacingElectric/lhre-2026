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
#include "adc.h"
#include "longhorn/rtos/logger.h"
#include "usbd_cdc_if.h"
#include "tim.h"
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
extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern ADC_HandleTypeDef hadc3;
/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
    .name = "defaultTask",
    .priority = (osPriority_t)osPriorityNormal,
    .stack_size = 128 * 4};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void StartADCTask(void *argument);
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
 * @brief  FreeRTOS initialization
 * @param  None
 * @retval None
 */
void MX_FREERTOS_Init(void)
{
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

  osThreadId_t adcTaskHandle;

  const osThreadAttr_t adcTask_attributes = {
      .name = "ADC_Task",
      .priority = osPriorityNormal,
      .stack_size = 512 * 4};

  adcTaskHandle = osThreadNew(StartADCTask, NULL, &adcTask_attributes);

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
  if (init_logging(CDC_Transmit_FS) == -1)
  {
    // USB failed
    Error_Handler();
  }

  /* Infinite loop */

  for (;;)
  {
    osDelay(1000);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
void StartADCTask(void *argument)
{
  uint32_t adc_main = 0;
  uint32_t adc_pb0 = 0;
  uint32_t adc_pb1 = 0;
  uint32_t adc_pb2 = 0;

  TickType_t last_wake = xTaskGetTickCount();

  for (;;)
  {
    /* -------- ADC3 (Existing sensor) -------- */
    HAL_ADC_Start(&hadc3);
    HAL_ADC_PollForConversion(&hadc3, 10);
    adc_main = HAL_ADC_GetValue(&hadc3);
    HAL_ADC_Stop(&hadc3);

    /* -------- ADC2 PB0 -------- */
    HAL_ADC_Start(&hadc2);
    HAL_ADC_PollForConversion(&hadc2, 10);
    adc_pb0 = HAL_ADC_GetValue(&hadc2);
    HAL_ADC_Stop(&hadc2);

    /* -------- ADC1 scan (PB1 + PB2) -------- */
    HAL_ADC_Start(&hadc1);

    HAL_ADC_PollForConversion(&hadc1, 10);
    adc_pb1 = HAL_ADC_GetValue(&hadc1);

    HAL_ADC_PollForConversion(&hadc1, 10);
    adc_pb2 = HAL_ADC_GetValue(&hadc1);

    HAL_ADC_Stop(&hadc1);

    /* -------- Convert ALL sensors to temperature -------- */

    float mv_main = ((float)adc_main / 4095.0f) * 3300.0f;
    float mv_pb0 = ((float)adc_pb0 / 4095.0f) * 3300.0f;
    float mv_pb1 = ((float)adc_pb1 / 4095.0f) * 3300.0f;
    float mv_pb2 = ((float)adc_pb2 / 4095.0f) * 3300.0f;

    /* Assuming LM35-style sensor:
       10 mV per °C
       500 mV offset
       temp_C = (mV - 500) / 10
    */

    float temp_main_c = (mv_main - 500.0f) / 10.0f;
    float temp_pb0_c = (mv_pb0 - 500.0f) / 10.0f;
    float temp_pb1_c = (mv_pb1 - 500.0f) / 10.0f;
    float temp_pb2_c = (mv_pb2 - 500.0f) / 10.0f;

    float temp_main_f = (temp_main_c * 9.0f / 5.0f) + 32.0f;
    float temp_pb0_f = (temp_pb0_c * 9.0f / 5.0f) + 32.0f;
    float temp_pb1_f = (temp_pb1_c * 9.0f / 5.0f) + 32.0f;
    float temp_pb2_f = (temp_pb2_c * 9.0f / 5.0f) + 32.0f;

    /* -------- RPM + Flow -------- */

    uint32_t tach_count = tach_pulse_count;
    uint32_t flow_count = flow_pulse_count;

    tach_pulse_count = 0;
    flow_pulse_count = 0;

    float rpm = (tach_count / 2.0f) * 60.0f;        // 2 pulses per rev
    float flow_lpm = (flow_count / 169.0f) * 60.0f; // adjust if needed

    /* Print only main temp if desired */
    log_printf(LOG_INFO,
               "T1: %.1fC  T2: %.1fC  T3: %.1fC  T4: %.1fC  RPM: %.0f  Flow: %.2f L/min\r\n",
               temp_main_c,
               temp_pb0_c,
               temp_pb1_c,
               temp_pb2_c,
               rpm,
               flow_lpm);

    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1000));
  }
}

/* USER CODE END Application */
