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
#include "tsm_can.h"

#include "usb_device.h"
#include <math.h>

#define ADC_MAX 4095.0f
#define VREF 3.3f
#define R_FIXED 10000.0f  // 10k resistor in divider
#define THERM_R0 10000.0f // thermistor resistance at 25C
#define THERM_BETA 3950.0f
#define TEMP_REF_K 298.15f // 25C in Kelvin

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "longhorn/rtos/led.h"
#include "longhorn/rtos/logger.h"
#include "longhorn/rtos/usb.h"
#include "longhorn/usb_base.h"
#include "tim.h"
#include "usbd_cdc_if.h"

#include "adc.h"
#include "gpio.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
volatile uint32_t flow_pulses = 0;
volatile uint32_t fan_pulses = 0;

float coolant_flow_lpm = 0;
float fan_rpm = 0;
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
    .stack_size = 128 * 4};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void StartSensorTask(void *argument);
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
 * @brief  FreeRTOS initialization
 * @param  None
 * @retval None
 */

osThreadId_t sensorTaskHandle;

const osThreadAttr_t sensorTask_attributes = {
    .name = "sensorTask",
    .priority = (osPriority_t)osPriorityNormal,
    .stack_size = 256 * 4};

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
      .pwm_start = (HAL_PWM_Start_Fn)HAL_TIM_PWM_Start,
      .timer_handle = &htim2,
  };

  led_init(&led);
  led_start_thread();

  sensorTaskHandle = osThreadNew(StartSensorTask, NULL, &sensorTask_attributes);
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
void StartDefaultTask(void *argument) {
  /* init code for USB_Device */
  MX_USB_Device_Init();
  /* USER CODE BEGIN StartDefaultTask */

  if (init_logging(CDC_Transmit_FS) == -1) {
    // If USB logging fails, stop LED thread so we notice
    // osThreadTerminate(ledHandle);
  }
  /* Infinite loop */

  for (;;) {
    osDelay(1000);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

// thermistor to adc

float thermistor_adc_to_temp(uint16_t adc) {
  if (adc == 0 || adc >= ADC_MAX)
    return -100.0f;

  /* Convert ADC → voltage */
  float v = (adc / ADC_MAX) * VREF;

  /* Voltage divider: thermistor on top, 10k to ground */
  float r_therm = R_FIXED * ((VREF / v) - 1.0f);

  /* Beta equation */
  float temp_k = 1.0f / ((1.0f / TEMP_REF_K) +
                         (1.0f / THERM_BETA) * logf(r_therm / THERM_R0));

  return temp_k - 273.15f;
}

/* placeholder for ambient waterproof */

float ds18b20_read_temp(void) {
  /* placeholder value so CAN works during testing */
  return 25.0f;
}

void StartSensorTask(void *argument) {
  uint32_t last_flow = 0;
  uint32_t last_fan = 0;

  for (;;) {
    uint16_t therm_adc[3];
    float temps_c[4];

    // adc thermistors

    HAL_ADC_Start(&hadc1);

    for (int i = 0; i < 3; i++) {
      HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
      therm_adc[i] = HAL_ADC_GetValue(&hadc1);
    }

    HAL_ADC_Stop(&hadc1);

    // convert to temp

    for (int i = 0; i < 3; i++) {
      temps_c[i] = thermistor_adc_to_temp(therm_adc[i]);
    }

    // ambient waterproof sensor
    temps_c[3] = ds18b20_read_temp();

    // flow rate

    uint32_t flow_now = flow_pulses;
    uint32_t flow_delta = flow_now - last_flow;
    last_flow = flow_now;

    // koolance coolant flow

    coolant_flow_lpm = (flow_delta * 60.0f) / 169.0f;

    // fan rpm
    uint32_t fan_now = fan_pulses;
    uint32_t fan_delta = fan_now - last_fan;
    last_fan = fan_now;

    /* most fans = 2 pulses per revolution */

    fan_rpm = (fan_delta * 60.0f) / 2.0f;

    // send to can

    tsm_can_update_sensors(temps_c[0], temps_c[1], temps_c[2], temps_c[3],
                           coolant_flow_lpm, fan_rpm);

    osDelay(100);
  }
}