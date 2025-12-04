/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : app_freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  */
/* USER CODE END Header */

#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

#include "usb_device.h"

/* USER CODE BEGIN Includes */
#include "rtos/dfu.h"
#include "rtos/led.h"
#include "rtos/logger.h"
#include "rtos/usb.h"
#include "tim.h"
#include "usb_base.h"
#include "usbd_cdc_if.h"
#include "adc.h"
#include <math.h>
#include <stdbool.h>

extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern ADC_HandleTypeDef hadc3;

// Declared DMA buffer declared in adc.c
extern volatile uint16_t adc3_dma_buf[2];
/* USER CODE END Includes */

/* USER CODE BEGIN PTD */
void StartDefaultTask(void *argument);
void StartDefaultTask2(void *argument);
void StartADCTask(void *argument);
void StartTorqueTask(void *argument);

typedef struct {
    uint16_t min_adc;
    uint16_t max_adc;
} apps_cal_t;
/* USER CODE END PTD */

/* USER CODE BEGIN PD */

// APPS raw ranges
#define APPS1_MIN_ADC   782u
#define APPS1_MAX_ADC   3262u
#define APPS2_MIN_ADC   382u
#define APPS2_MAX_ADC   1586u

// Torque limit for motor spin
#define TORQUE_MAX_NM   5.0f

// Rules parameters
#define APPS_MIN_TRAVEL_FOR_CHECK   0.10f    //10%
#define APPS_MAX_DIFF_ALLOWED       0.10f   
#define APPS_IMPLAUS_TIME_MS        100u     // By rules

// Task rate for torque task
#define TORQUE_TASK_PERIOD_MS       50u
#define APPS_IMPLAUS_COUNT  (APPS_IMPLAUS_TIME_MS / TORQUE_TASK_PERIOD_MS)

// Arbitrary pedal low-pass filtering
#define PEDAL_FILTER_ALPHA          0.4f

/* USER CODE END PD */

/* USER CODE BEGIN PM */
static float apps_adc_to_travel(uint16_t raw, const apps_cal_t *cal)
{
    if (raw < cal->min_adc) raw = cal->min_adc;
    if (raw > cal->max_adc) raw = cal->max_adc;

    float span = (float)(cal->max_adc - cal->min_adc);
    if (span <= 1.0f) return 0.0f;

    return ((float)raw - (float)cal->min_adc) / span;
}
/* USER CODE END PM */

/* USER CODE BEGIN Variables */
osThreadId_t defaultTaskHandle;
osThreadId_t defaultTask2Handle;
osThreadId_t ledHandle;
osThreadId_t adcTaskHandle;
osThreadId_t torqueTaskHandle;
/* USER CODE END Variables */

// Definitions for defaultTask 
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .priority = osPriorityNormal,
  .stack_size = 128 * 4
};

/* USER CODE BEGIN ThreadsAttr */
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
/* USER CODE END ThreadsAttr */

/**
  * @brief  FreeRTOS initialization
  * @retval None
  */
void MX_FREERTOS_Init(void) {

// Core tasks
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
  /* USER CODE END RTOS_THREADS */
}

// DefaultTask: USB Init + DFU Thread
void StartDefaultTask(void *argument)
{
    // USB MUST init inside a running RTOS task don't mess with this Alice you've done fucked up once
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

// StartADCTask: starts DMA and prints raw ADC values
void StartADCTask(void *argument)
{
    // Start DMA once for ADC3 (2 channels: CH9, CH10) dual sensor APPS W
    if (HAL_ADC_Start_DMA(&hadc3, (uint32_t*)adc3_dma_buf, 2) != HAL_OK) {
        Error_Handler();
    }

    uint32_t adc1_val = 0;
    uint32_t adc2_val = 0;

    for (;;)
    {
        // TO-DO: ADC1 polling empty for now until test BSE
        HAL_ADC_Start(&hadc1);
        if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)
            adc1_val = HAL_ADC_GetValue(&hadc1);
        HAL_ADC_Stop(&hadc1);

        // TO-DO: ADC2 polling for reading BSPD status
        HAL_ADC_Start(&hadc2);
        if (HAL_ADC_PollForConversion(&hadc2, 10) == HAL_OK)
            adc2_val = HAL_ADC_GetValue(&hadc2);
        HAL_ADC_Stop(&hadc2);

        // TO-DO: Trigger one ADC3 scan (2 channels → DMA into adc3_dma_buf)
        HAL_ADC_Start(&hadc3);
        osDelay(1);    // allow DMA transfer to complete

        uint16_t apps_ch9  = adc3_dma_buf[0];   // APPS1
        uint16_t apps_ch10 = adc3_dma_buf[1];   // APPS2

        ts_printf("ADC1:%lu  ADC2:%lu  APPS9:%u  APPS10:%u\r\n",
                  adc1_val, adc2_val, apps_ch9, apps_ch10);

        osDelay(pdMS_TO_TICKS(300));
    }
}

// Torque Task: dual APPS, plausibility, 0–5 Nm mapping
void StartTorqueTask(void *argument)
{
    // Let DMA + ADC settle for a bit
    osDelay(pdMS_TO_TICKS(200));

    const apps_cal_t apps1 = { APPS1_MIN_ADC, APPS1_MAX_ADC };
    const apps_cal_t apps2 = { APPS2_MIN_ADC, APPS2_MAX_ADC };

    float tq_cmd          = 0.0f;
    float pedal_filt      = 0.0f;

    uint8_t implaus_counter = 0;
    bool    apps_implaus    = false;

    bool brake_latched      = false;   // will be wired to BSE later

    for (;;)
    {
        uint16_t raw1 = adc3_dma_buf[0];   // APPS1 = CH9
        uint16_t raw2 = adc3_dma_buf[1];   // APPS2 = CH10

        // Convert each to 0–1 travel using calibration
        float p1 = apps_adc_to_travel(raw1, &apps1);
        float p2 = apps_adc_to_travel(raw2, &apps2);

        // Clamp
        if (p1 < 0.0f) p1 = 0.0f; if (p1 > 1.0f) p1 = 1.0f;
        if (p2 < 0.0f) p2 = 0.0f; if (p2 > 1.0f) p2 = 1.0f;

        float p_max = (p1 > p2) ? p1 : p2;
        float diff  = fabsf(p1 - p2);

        // Plausibility: only when pedal > 10%
        if (p_max > APPS_MIN_TRAVEL_FOR_CHECK)
        {
            if (diff > APPS_MAX_DIFF_ALLOWED)
            {
                if (implaus_counter < 255)
                    implaus_counter++;
            }
            else
            {
                implaus_counter = 0;
            }
        }
        else
        {
            implaus_counter = 0;
        }

        // Implausible if mismatch persists for >100 ms
        if (implaus_counter >= APPS_IMPLAUS_COUNT)
            apps_implaus = true;

        // Reset implausibility once both pedals basically released
        if (apps_implaus && p1 < 0.05f && p2 < 0.05f)
        {
            apps_implaus    = false;
            implaus_counter = 0;
        }

        // Fuse two APPS into one pedal command when valid
        float pedal = 0.0f;
        if (!apps_implaus)
            pedal = 0.5f * (p1 + p2);

        if (pedal < 0.0f) pedal = 0.0f;
        if (pedal > 1.0f) pedal = 1.0f;

        // Optional low-pass filter on pedal ? 
        pedal_filt += PEDAL_FILTER_ALPHA * (pedal - pedal_filt);

        // Brake + APPS rule (replace brake_active with real BSE at Pickle)
        bool brake_active = false;  // TODO: wire to BSE inputs when available

        if (brake_active && pedal_filt > 0.25f)
            brake_latched = true;

        if (brake_latched && pedal_filt < 0.05f)
            brake_latched = false;

        // Final torque command (0–5 Nm)
        if (apps_implaus || brake_latched)
            tq_cmd = 0.0f;
        else
            tq_cmd = pedal_filt * TORQUE_MAX_NM;

        // Debug printing
        int p1_i  = (int)(p1 * 1000.0f);
        int p2_i  = (int)(p2 * 1000.0f);
        int pf_i  = (int)(pedal_filt * 1000.0f);
        int tq_i  = (int)(tq_cmd * 100.0f);

        ts_printf("APP1=%u (%d.%03d)  APP2=%u (%d.%03d)  ped_f=%d.%03d  tq=%d.%02d Nm  impl=%d  brake=%d\r\n",
                  raw1, p1_i/1000, p1_i%1000,
                  raw2, p2_i/1000, p2_i%1000,
                  pf_i/1000, pf_i%1000,
                  tq_i/100, tq_i%100,
                  apps_implaus ? 1 : 0,
                  brake_latched ? 1 : 0);

        osDelay(pdMS_TO_TICKS(TORQUE_TASK_PERIOD_MS));
    }
}
