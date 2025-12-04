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
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

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
#include "adc.h"

extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern ADC_HandleTypeDef hadc3;

/* DMA buffer declared in adc.c */
extern volatile uint16_t adc3_dma_buf[2];
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
void StartDefaultTask(void *argument);
void StartDefaultTask2(void *argument);
void StartADCTask(void *argument);
void StartTorqueTask(void *argument);
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define APPS_MIN_ADC      815
#define APPS_MAX_ADC      3390
#define MAX_TORQUE_NM     80.0f
#define TORQUE_ALPHA      0.5f
/* USER CODE END PD */

/* Definitions for defaultTask */
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .priority = osPriorityNormal,
  .stack_size = 128 * 4
};

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

/**
  * @brief  FreeRTOS initialization
  * @retval None
  */
void MX_FREERTOS_Init(void) {

  /* Create core tasks */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
    // LED PWM setup
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
    adcTaskHandle      = osThreadNew(StartADCTask, NULL, &adcTask_attributes);
    torqueTaskHandle   = osThreadNew(StartTorqueTask, NULL, &torqueTask_attributes);

    /* ❌ DO NOT START ADC3 DMA HERE — breaks USB + LED */
  /* USER CODE END RTOS_THREADS */
}

/* DefaultTask: USB Init + DFU Thread */
void StartDefaultTask(void *argument)
{
    /* USB MUST init inside a running RTOS task */
    MX_USB_Device_Init();

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

void StartDefaultTask2(void *argument)
{
    for (;;) {
        osDelay(pdMS_TO_TICKS(1000));
    }
}

/* StartADCTask: now responsible for launching DMA safely */
void StartADCTask(void *argument)
{
    /* ✅ Correct place to start ADC3 DMA */
    // if (HAL_ADC_Start_DMA(&hadc3, (uint32_t*)adc3_dma_buf, 2) != HAL_OK) {
    //     Error_Handler();
    // }

    /* Start DMA ONCE */
HAL_ADC_Start_DMA(&hadc3, (uint32_t*)adc3_dma_buf, 2);



    uint32_t adc1_val = 0;
    uint32_t adc2_val = 0;

for (;;) {

    /* Trigger one ADC3 scan (2 channels) */
    HAL_ADC_Start(&hadc3);

    /* Optional: small delay to let DMA finish */
    osDelay(1);

    uint16_t apps_ch9  = adc3_dma_buf[0];
    uint16_t apps_ch10 = adc3_dma_buf[1];

    ts_printf("ADC1:%lu  ADC2:%lu  APPS9:%u  APPS10:%u",
              adc1_val, adc2_val, apps_ch9, apps_ch10);

    osDelay(pdMS_TO_TICKS(300));
}

}

/* Torque Task: uses DMA values */
void StartTorqueTask(void *argument)
{
    osDelay(pdMS_TO_TICKS(1000));

    float raw_filt = (float)APPS_MIN_ADC;
    float tq_filt  = 0.0f;

    for (;;) {

        uint16_t raw = adc3_dma_buf[0]; // primary pedal

        if (raw < APPS_MIN_ADC) raw = APPS_MIN_ADC;
        if (raw > APPS_MAX_ADC) raw = APPS_MAX_ADC;

        raw_filt = raw_filt + TORQUE_ALPHA * ((float)raw - raw_filt);

        float pct = (raw_filt - APPS_MIN_ADC) / (float)(APPS_MAX_ADC - APPS_MIN_ADC);
        if (pct < 0) pct = 0;
        if (pct > 1) pct = 1;

        float tq_raw = pct * MAX_TORQUE_NM;
        tq_filt = tq_raw;

        static uint32_t last_print = 0;
        uint32_t now = osKernelGetTickCount();

        if (now - last_print > 250) {
            int pct_i = (int)(pct * 1000.0f);
            int tq_i  = (int)(tq_filt * 100.0f);

            ts_printf("APPS=%u  pct=%d.%03d  torque=%d.%02d Nm",
                      raw, pct_i / 1000, pct_i % 1000, tq_i / 100, tq_i % 100);

            last_print = now;
        }

        osDelay(pdMS_TO_TICKS(50));
    }
} 