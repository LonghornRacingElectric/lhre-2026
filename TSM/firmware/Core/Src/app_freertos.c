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
#include "longhorn/rtos/dfu.h"
#include "longhorn/rtos/led.h"
#include "longhorn/rtos/logger.h"
#include "longhorn/rtos/usb.h"
#include "longhorn/usb_base.h"
#include "ota/ota_flash.h"
#include "tim.h"
#include "tsm_can.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"
#include <cmsis_os2.h>
#include <math.h>

#include "adc.h"
#include "gpio.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define ADC_MAX 4095.0f
#define VREF 3.3f
#define R_FIXED 10000.0f   // 10k resistor in divider
#define THERM_R0 10000.0f  // thermistor resistance at 25C
#define THERM_BETA 3977.0f // beta constant
#define TEMP_REF_K 298.15f // 25C in Kelvin

#define DS18B20_PORT GPIOA
#define DS18B20_PIN GPIO_PIN_3

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

  sensorTaskHandle = osThreadNew(StartSensorTask, NULL, &sensorTask_attributes);

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

  // log_printf(LOG_INFO, "[TSM] Application started\n");

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
  float v = ((float)adc / ADC_MAX) * VREF;

  /* Voltage divider: 10k to VREF, thermistor to GND */
  float r_therm = R_FIXED * (v / (VREF - v));

  /* Beta equation */
  float temp_k = 1.0f / ((1.0f / TEMP_REF_K) +
                         (1.0f / THERM_BETA) * logf(r_therm / THERM_R0));

  return temp_k - 273.15f;
}

// ambient waterproof

static void delay_us(uint32_t us) {
  // Use DWT cycle counter or a timer
  uint32_t start = DWT->CYCCNT;
  uint32_t ticks = us * (SystemCoreClock / 1000000);
  while ((DWT->CYCCNT - start) < ticks)
    ;
}

static inline void ds18b20_low(void) {
  HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_RESET);
}

static inline void ds18b20_release(void) {
  HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_SET);
}

static inline uint8_t ds18b20_read_pin(void) {
  return HAL_GPIO_ReadPin(DS18B20_PORT, DS18B20_PIN);
}

uint8_t ds18b20_reset(void) {
  ds18b20_low();
  delay_us(480);
  ds18b20_release();
  delay_us(70); // wait for presence pulse
  uint8_t presence = !ds18b20_read_pin();
  delay_us(410); // remainder of reset slot
  return presence;
}

void ds18b20_write_bit(uint8_t bit) {
  ds18b20_low();
  if (bit) {
    delay_us(1);
    ds18b20_release();
    delay_us(60);
  } else {
    delay_us(60);
    ds18b20_release();
    delay_us(1);
  }
}

uint8_t ds18b20_read_bit(void) {
  ds18b20_low();
  delay_us(1);
  ds18b20_release();
  delay_us(10);
  uint8_t bit = ds18b20_read_pin();
  delay_us(55);
  return bit;
}

uint8_t ds18b20_read_byte(void) {
  uint8_t value = 0;

  for (int i = 0; i < 8; i++) {
    value |= (ds18b20_read_bit() << i);
  }

  return value;
}

float ds18b20_read_temp(void) {
  if (!ds18b20_reset())
    return -100;

  /* Skip ROM */
  for (int i = 0; i < 8; i++)
    ds18b20_write_bit((0xCC >> i) & 1);

  /* Convert temperature */
  for (int i = 0; i < 8; i++)
    ds18b20_write_bit((0x44 >> i) & 1);

  osDelay(750);

  ds18b20_reset();

  for (int i = 0; i < 8; i++)
    ds18b20_write_bit((0xCC >> i) & 1);

  for (int i = 0; i < 8; i++)
    ds18b20_write_bit((0xBE >> i) & 1);

  uint8_t temp_l = ds18b20_read_byte();
  uint8_t temp_h = ds18b20_read_byte();

  // log_printf(LOG_INFO, "raw: %02X %02X\r\n", temp_h, temp_l);

  int16_t raw = (temp_h << 8) | temp_l;

  return raw / 16.0f;
}

void StartSensorTask(void *argument) {

  uint32_t last_flow = 0;
  uint32_t last_fan = 0;

  for (;;) {

    uint16_t therm_adc[3];
    float temps_c[4];

    /* ======================
       Read thermistors
       ====================== */

    /* ADC1 reads PB0 + PB1 */
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
    sConfig.SingleDiff = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset = 0;
    sConfig.Rank = ADC_REGULAR_RANK_1;

    // PB0 = IN15 = motor
    sConfig.Channel = ADC_CHANNEL_15;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);
    therm_adc[0] = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);

    // PB1 = IN12 = inverter
    sConfig.Channel = ADC_CHANNEL_12;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);
    therm_adc[1] = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);

    // adc2 for pb2

    HAL_ADC_Start(&hadc2);
    HAL_ADC_PollForConversion(&hadc2, 10);
    therm_adc[2] = HAL_ADC_GetValue(&hadc2);
    HAL_ADC_Stop(&hadc2);

    // log_printf(LOG_INFO, "ADC volts: %.3f %.3f %.3f\r\n",
    //            (therm_adc[0] / 4095.0f) * 3.3f, (therm_adc[1] / 4095.0f)
    //            * 3.3f, (therm_adc[2] / 4095.0f) * 3.3f);

    /* Convert temperatures */

    for (int i = 0; i < 3; i++) {
      temps_c[i] = thermistor_adc_to_temp(therm_adc[i]);
    }

    // Ambient waterproof sensor
    temps_c[3] = ds18b20_read_temp();

    /* ======================
       Flow sensor
       ====================== */

    uint32_t flow_now = flow_pulses;
    uint32_t flow_delta = flow_now - last_flow;
    last_flow = flow_now;

    /* Koolance flow sensor: 169 pulses per liter */

    coolant_flow_lpm = (flow_delta * 60.0f) / 169.0f;

    /* ======================
       Fan RPM
       ====================== */

    uint32_t fan_now = fan_pulses;
    uint32_t fan_delta = fan_now - last_fan;
    last_fan = fan_now;

    /* Most fans = 2 pulses per revolution */

    fan_rpm = (fan_delta * 60.0f) / 2.0f;

    /* ======================
       Send CAN packets
       ====================== */

    tsm_can_update_coolant_loop(temps_c[0],  // after motor
                                temps_c[1],  // after inverter
                                temps_c[2]); // after radiator

    tsm_can_update_cooling_system(fan_rpm, coolant_flow_lpm, temps_c[3]);
    // logging
    log_printf(LOG_INFO,
               "Motor: %.1f C | Inverter: %.1f C | Radiator: %.1f C | Ambient: "
               "%.1f C | Flow: %.2f LPM | Fan: %.0f RPM\r\n",
               temps_c[0], temps_c[1], temps_c[2], temps_c[3], coolant_flow_lpm,
               fan_rpm);

    osDelay(pdMS_TO_TICKS(300));

    tsm_can_debug();
  }
}

/* USER CODE END Application */

