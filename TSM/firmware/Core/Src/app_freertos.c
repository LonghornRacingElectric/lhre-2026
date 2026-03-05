/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name          : app_freertos.c
 * Description        : FreeRTOS application tasks
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
#include "longhorn/rtos/led.h"
#include "longhorn/rtos/logger.h"

#include "adc.h"
#include "tim.h"
#include "usbd_cdc_if.h"

/* USER CODE BEGIN Includes */

/* Pulse counters from interrupts */
volatile uint32_t tach_pulse_count = 0;
volatile uint32_t flow_pulse_count = 0;

/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/

extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern TIM_HandleTypeDef htim2;

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;

const osThreadAttr_t defaultTask_attributes = {
    .name = "defaultTask",
    .priority = (osPriority_t)osPriorityNormal,
    .stack_size = 128 * 4};

/* Private function prototypes -----------------------------------------------*/

void StartDefaultTask(void *argument);
void StartADCTask(void *argument);

/* ---------------------------------------------------------- */
/* FreeRTOS Initialization                                    */
/* ---------------------------------------------------------- */

void MX_FREERTOS_Init(void)
{

  /* Create default task */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* ---------- LED THREAD ---------- */

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

  /* ---------- ADC TASK ---------- */

  const osThreadAttr_t adcTask_attributes = {
      .name = "ADC_Task",
      .priority = osPriorityNormal,
      .stack_size = 512 * 4};

  osThreadNew(StartADCTask, NULL, &adcTask_attributes);
}

/* ---------------------------------------------------------- */
/* Default Task (USB + Logging)                               */
/* ---------------------------------------------------------- */

void StartDefaultTask(void *argument)
{
  MX_USB_Device_Init();

  if (init_logging(CDC_Transmit_FS) == -1)
  {
    Error_Handler();
  }

  for (;;)
  {
    osDelay(1000);
  }
}

/* ---------------------------------------------------------- */
/* ADC + Sensor Task                                          */
/* ---------------------------------------------------------- */

void StartADCTask(void *argument)
{
  uint32_t adc1_value = 0;
  uint32_t adc2_value = 0;

  TickType_t last_wake = xTaskGetTickCount();

  for (;;)
  {
    /* -------- ADC1 -------- */

    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);
    adc1_value = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);

    /* -------- ADC2 -------- */

    HAL_ADC_Start(&hadc2);
    HAL_ADC_PollForConversion(&hadc2, 10);
    adc2_value = HAL_ADC_GetValue(&hadc2);
    HAL_ADC_Stop(&hadc2);

    /* -------- Convert to millivolts -------- */

    float mv1 = ((float)adc1_value / 4095.0f) * 3300.0f;
    float mv2 = ((float)adc2_value / 4095.0f) * 3300.0f;

    /* Example LM35 conversion */

    float temp1_c = (mv1 - 500.0f) / 10.0f;
    float temp2_c = (mv2 - 500.0f) / 10.0f;

    /* -------- RPM + Flow -------- */

    uint32_t tach_count = tach_pulse_count;
    uint32_t flow_count = flow_pulse_count;

    tach_pulse_count = 0;
    flow_pulse_count = 0;

    float rpm = (tach_count / 2.0f) * 60.0f;
    float flow_lpm = (flow_count / 169.0f) * 60.0f;

    /* -------- Print data -------- */

    log_printf(LOG_INFO,
               "Temp1: %.1fC  Temp2: %.1fC  RPM: %.0f  Flow: %.2f L/min\r\n",
               temp1_c,
               temp2_c,
               rpm,
               flow_lpm);

    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1000));
  }
}
