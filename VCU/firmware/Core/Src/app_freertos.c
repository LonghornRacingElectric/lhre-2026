/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name          : app_freertos.c
 * Description        : Code for freertos applications
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "main.h"
#include "task.h"
#include "usb_device.h"
#include <math.h>

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
void StartTorqueTask(void *argument);
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define APPS_MIN_ADC      815 // arbitrary pedal tuning
#define APPS_MAX_ADC      3390 
#define MAX_TORQUE_NM     80.0f
#define TORQUE_ALPHA      0.5f // new_value = old_value + smoothing * (new_input - old_value) 

/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
osThreadId_t defaultTaskHandle;
osThreadId_t defaultTask2Handle;
osThreadId_t ledHandle;
osThreadId_t adcTaskHandle;
osThreadId_t torqueTaskHandle;

const osThreadAttr_t adcTask_attributes = {
    .name = "ADC_Task",
    .priority = osPriorityNormal,
    .stack_size = 2048
};

const osThreadAttr_t torqueTask_attributes = {
    .name = "TorqueTask",
    .priority = osPriorityAboveNormal,
    .stack_size = 2048
};
/* USER CODE END Variables */

/* Private function prototypes -----------------------------------------------*/
void StartDefaultTask(void *argument);
void StartDefaultTask2(void *argument);
void StartADCTask(void *argument);

/* USER CODE BEGIN FunctionPrototypes */
/* USER CODE END FunctionPrototypes */

void MX_FREERTOS_Init(void) {

    defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, NULL);

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

    defaultTask2Handle = osThreadNew(StartDefaultTask2, NULL, NULL);

    adcTaskHandle = osThreadNew(StartADCTask, NULL, &adcTask_attributes);

    torqueTaskHandle = osThreadNew(StartTorqueTask, NULL, &torqueTask_attributes);
    /* USER CODE END RTOS_THREADS */
}

/* USER CODE BEGIN StartDefaultTask */
void StartDefaultTask(void *argument) {

    MX_USB_Device_Init();   // MUST happen before logging

    if (init_logging(CDC_Transmit_FS) == -1) {
        osThreadTerminate(ledHandle);
    }

    dfu_config dfu = {
        .delay_fn    = (Delay_fn)osDelay,
        .gpiox       = GPIOB,
        .pin         = GPIO_PIN_7,
        .pin_set_fn  = (PinSet_fn)HAL_GPIO_WritePin,
        .reset_fn    = (SystemReset_fn)HAL_NVIC_SystemReset,
    };

    init_dfu(dfu);
    dfu_start_thread();

    for (;;) {
        osDelay(pdMS_TO_TICKS(500));
    }
}
/* USER CODE END StartDefaultTask */

void StartDefaultTask2(void *argument) {
    for (;;) {
        osDelay(pdMS_TO_TICKS(1000));
    }
}

/* USER CODE BEGIN ADC_Task */
void StartADCTask(void *argument) {
    uint32_t adc1_val = 0;
    uint32_t adc2_val = 0;
    uint32_t adc3_val = 0;

    for (;;) {
        HAL_ADC_Start(&hadc1);
        if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)
            adc1_val = HAL_ADC_GetValue(&hadc1);
        HAL_ADC_Stop(&hadc1);

        HAL_ADC_Start(&hadc2);
        if (HAL_ADC_PollForConversion(&hadc2, 10) == HAL_OK)
            adc2_val = HAL_ADC_GetValue(&hadc2);
        HAL_ADC_Stop(&hadc2);

        HAL_ADC_Start(&hadc3);
        if (HAL_ADC_PollForConversion(&hadc3, 10) == HAL_OK)
            adc3_val = HAL_ADC_GetValue(&hadc3);
        HAL_ADC_Stop(&hadc3);

        ts_printf("ADC1:%lu  ADC2:%lu  APPS:%lu", adc1_val, adc2_val, adc3_val);

        osDelay(pdMS_TO_TICKS(300));
    }
}
/* USER CODE END ADC_Task */


// new torque task
void StartTorqueTask(void *argument) {
    osDelay(pdMS_TO_TICKS(1000));  // allow system to start

    float raw_filt = APPS_MIN_ADC; // filtered raw ADC
    float tq_filt  = 0.0f;         // filtered torque

    for (;;) {
        HAL_ADC_Start(&hadc3);
        if (HAL_ADC_PollForConversion(&hadc3, 10) == HAL_OK) {
            uint32_t raw = HAL_ADC_GetValue(&hadc3);
            HAL_ADC_Stop(&hadc3);

            // Clamp raw ADC
            if (raw < APPS_MIN_ADC) raw = APPS_MIN_ADC;
            if (raw > APPS_MAX_ADC) raw = APPS_MAX_ADC;

            // Low-pass filter on raw ADC
            raw_filt = raw_filt + TORQUE_ALPHA * ((float)raw - raw_filt);

            // Convert to percent
            float pct = (raw_filt - APPS_MIN_ADC) / (float)(APPS_MAX_ADC - APPS_MIN_ADC);
            if (pct < 0.0f) pct = 0.0f;
            if (pct > 1.0f) pct = 1.0f;

            // Calculate torque
            float tq_raw = pct * MAX_TORQUE_NM;

            // Low-pass filter torque (optional)
            tq_filt = tq_raw; // instant torque from filtered ADC

            // Print every 250ms
            static uint32_t last_print = 0;
            uint32_t now = osKernelGetTickCount();
            if (now - last_print > 250) {
                int pct_i = (int)(pct * 1000);
                int tq_i  = (int)(tq_filt * 100);

                ts_printf("APPS=%lu  pct=%d.%03d  torque=%d.%02d Nm",
                          raw, pct_i / 1000, pct_i % 1000, tq_i / 100, tq_i % 100);

                last_print = now;
            }
        } else {
            HAL_ADC_Stop(&hadc3);
        }

        osDelay(50);
    }
}
