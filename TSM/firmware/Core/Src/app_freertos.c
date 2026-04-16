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
#include "flow_fan.h"
#include "longhorn/rtos/dfu.h"
#include "longhorn/rtos/led.h"
#include "longhorn/rtos/logger.h"
#include "longhorn/rtos/usb.h"
#include "longhorn/usb_base.h"
#include "ota/ota_flash.h"
#include "stm32g4xx_hal.h"
#include "tim.h"
#include "tsm_can.h"
#include "tsm_sensors.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"
#include <math.h>

#include "adc.h"
#include "fdcan.h"
#include "gpio.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
volatile uint32_t flow_pulses = 0;
volatile uint32_t fan_pulses = 0;
volatile uint32_t bat_fan_pulses = 0;

float coolant_flow_lpm = 0;
float rad_fan_rpm = 0;
float bat_fan_rpm = 0;
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

osThreadId_t sensorTaskHandle;
const osThreadAttr_t sensorTask_attributes = {
    .name = "sensorTask",
    .priority = (osPriority_t)osPriorityNormal,
    .stack_size = 2048 * 4};
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
void StartSensorTask(void *argument);
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
  osDelay(500);
  if (init_logging(CDC_Transmit_FS) == -1) {
    Error_Handler();
  }

  log_printf(LOG_INFO, "BOOT\r\n");

  dfu_config dfu = {
      .delay_fn = (Delay_fn)osDelay,
      .gpiox = GPIOB,
      .pin = GPIO_PIN_9,
      .pin_set_fn = (PinSet_fn)HAL_GPIO_WritePin,
      .reset_fn = (SystemReset_fn)HAL_NVIC_SystemReset,
      .set_bank1_fn = (SetBank1_fn)ota_set_bank1,
  };

  init_dfu(dfu);
  dfu_start_thread();

  /* Initialize CAN */
  tsm_can_init();

  sensorTaskHandle = osThreadNew(StartSensorTask, NULL, &sensorTask_attributes);

  // log_printf(LOG_INFO, "[TSM] Application started\n");

  /* Infinite loop */
  for (;;) {
    osDelay(1000);
  }

  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
void StartSensorTask(void *argument) {
  uint32_t last_flow = 0;
  uint32_t last_rad_fan = 0;
  uint32_t last_bat_fan = 0;
  uint32_t last_tick = HAL_GetTick();

  // bool psr_logged = false;

  for (;;) {
    // log_printf(LOG_INFO, "FDCAN PSR: 0x%08lX\n", hfdcan2.Instance->PSR);

    uint16_t therm_adc[3];
    float temps_c[4];

    // Read thermistors via ADC
    therm_adc[0] = read_therm_adc(&hadc1, ADC_CHANNEL_15); // motor
    therm_adc[1] = read_therm_adc(&hadc1, ADC_CHANNEL_12); // inverter
    therm_adc[2] = read_therm_adc(&hadc2, ADC_CHANNEL_12); // radiator

    // Convert ADC → temperature
    for (int i = 0; i < 3; i++)
      temps_c[i] = thermistor_adc_to_temp(therm_adc[i]);

    // Ambient temp sensor
    temps_c[3] = ds18b20_read_temp();
    // temps_c[3] = 0.0f;

    // for (;;) {
    //   log_printf(LOG_INFO, "GPIO: PA5=%d PA6=%d PA7=%d\r\n",
    //              HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5),
    //              HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6),
    //              HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_7));
    //   osDelay(10);
    // }

    // Flow + fan update
    flow_fan_update(&last_flow, &last_rad_fan, &last_bat_fan, &coolant_flow_lpm,
                    &rad_fan_rpm, &bat_fan_rpm, &last_tick);

    // Send CAN packets
    tsm_can_update_coolant_loop(temps_c[0], temps_c[1], temps_c[2]);
    tsm_can_update_cooling_system(rad_fan_rpm, bat_fan_rpm, coolant_flow_lpm,
                                  temps_c[3]);

    // Logging
    char m_buf[8], i_buf[8], r_buf[8], a_buf[8];

    // log_printf(LOG_INFO,
    //            "ADC Raw | Motor: %4u | Inverter: %4u | Radiator: %4u\r\n",
    //            therm_adc[0], therm_adc[1], therm_adc[2]);
    // osDelay(100);

    log_printf(LOG_INFO,
               "Motor: %s C | Inverter: %s C | Radiator: %s C | Ambient: %s C "
               "| Flow: %.2f LPM | Rad Fan: %.0f RPM | Bat Fan: %.0f RPM\r\n",
               temp_to_str(temps_c[0], m_buf, sizeof(m_buf)),
               temp_to_str(temps_c[1], i_buf, sizeof(i_buf)),
               temp_to_str(temps_c[2], r_buf, sizeof(r_buf)),
               temp_to_str(temps_c[3], a_buf, sizeof(a_buf)), coolant_flow_lpm,
               rad_fan_rpm, bat_fan_rpm);

    osDelay(pdMS_TO_TICKS(1000));
    // tsm_can_debug();
    // osDelay(10);
  }
}

/* USER CODE END Application */

