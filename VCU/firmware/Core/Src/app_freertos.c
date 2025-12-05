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
#include <math.h>
#include <stdbool.h>

#include "adc.h"
#include "longhorn/rtos/dfu.h"
#include "longhorn/rtos/led.h"
#include "longhorn/rtos/logger.h"
#include "longhorn/rtos/usb.h"
#include "longhorn/usb_base.h"
#include "tim.h"
#include "usbd_cdc_if.h"

extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern ADC_HandleTypeDef hadc3;

extern volatile uint16_t adc3_dma_buf[2];
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
void StartDefaultTask(void* argument);
void StartDefaultTask2(void* argument);
void StartADCTask(void* argument);
void StartTorqueTask(void* argument);

typedef struct {
    uint16_t min_adc;
    uint16_t max_adc;
} apps_cal_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define APPS1_MIN_ADC 782u
#define APPS1_MAX_ADC 3262u
#define APPS2_MIN_ADC 382u
#define APPS2_MAX_ADC 1586u

#define TORQUE_MAX_NM 5.0f

#define APPS_MIN_TRAVEL_FOR_CHECK 0.10f
#define APPS_MAX_DIFF_ALLOWED 0.10f
#define APPS_IMPLAUS_TIME_MS 100u

#define TORQUE_TASK_PERIOD_MS 50u
#define APPS_IMPLAUS_COUNT (APPS_IMPLAUS_TIME_MS / TORQUE_TASK_PERIOD_MS)

#define PEDAL_FILTER_ALPHA 0.4f
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
static float apps_adc_to_travel(uint16_t raw, const apps_cal_t* cal) {
    if (raw < cal->min_adc) raw = cal->min_adc;
    if (raw > cal->max_adc) raw = cal->max_adc;
    float span = (float)(cal->max_adc - cal->min_adc);
    if (span <= 1.0f) return 0.0f;
    return ((float)raw - (float)cal->min_adc) / span;
}
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
osThreadId_t defaultTask2Handle;
osThreadId_t ledHandle;
osThreadId_t adcTaskHandle;
osThreadId_t torqueTaskHandle;
/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 128 * 4
};

/* USER CODE BEGIN ThreadsAttr */
const osThreadAttr_t adcTask_attributes = {
    .name = "ADC_Task",
    .priority = osPriorityNormal,
    .stack_size = 2048};

const osThreadAttr_t torqueTask_attributes = {
    .name = "TorqueTask",
    .priority = osPriorityAboveNormal,
    .stack_size = 2048};
/* USER CODE END ThreadsAttr */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void);

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {

  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
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

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  MX_USB_Device_Init();
  /* USER CODE BEGIN StartDefaultTask */

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
        osDelay(pdMS_TO_TICKS(500));
    }

  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_StartDefaultTask2 */
/* USER CODE END Header_StartDefaultTask2 */
void StartDefaultTask2(void* argument) {
    /* USER CODE BEGIN StartDefaultTask2 */
    for (;;) {
        osDelay(pdMS_TO_TICKS(1000));
    }
    /* USER CODE END StartDefaultTask2 */
}

/* USER CODE BEGIN Header_StartADCTask */
/* USER CODE END Header_StartADCTask */
void StartADCTask(void* argument) {
    /* USER CODE BEGIN StartADCTask */

    if (HAL_ADC_Start_DMA(&hadc3, (uint32_t*)adc3_dma_buf, 2) != HAL_OK) {
        Error_Handler();
    }

    uint32_t adc1_val = 0;
    uint32_t adc2_val = 0;

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
        osDelay(1);

        uint16_t apps_ch9 = adc3_dma_buf[0];
        uint16_t apps_ch10 = adc3_dma_buf[1];

        log_printf(LOG_INFO, "ADC1:%lu  ADC2:%lu  APPS9:%u  APPS10:%u\r\n",
                   adc1_val, adc2_val, apps_ch9, apps_ch10);

        osDelay(pdMS_TO_TICKS(300));
    }

    /* USER CODE END StartADCTask */
}

/* USER CODE BEGIN Header_StartTorqueTask */
/* USER CODE END Header_StartTorqueTask */
void StartTorqueTask(void* argument) {
    /* USER CODE BEGIN StartTorqueTask */

    osDelay(pdMS_TO_TICKS(200));

    const apps_cal_t apps1 = {APPS1_MIN_ADC, APPS1_MAX_ADC};
    const apps_cal_t apps2 = {APPS2_MIN_ADC, APPS2_MAX_ADC};

    float tq_cmd = 0.0f;
    float pedal_filt = 0.0f;

    uint8_t implaus_counter = 0;
    bool apps_implaus = false;

    bool brake_latched = false;

    for (;;) {
        uint16_t raw1 = adc3_dma_buf[0];
        uint16_t raw2 = adc3_dma_buf[1];

        float p1 = apps_adc_to_travel(raw1, &apps1);
        float p2 = apps_adc_to_travel(raw2, &apps2);

        if (p1 < 0.0f) p1 = 0.0f;
        if (p1 > 1.0f) p1 = 1.0f;
        if (p2 < 0.0f) p2 = 0.0f;
        if (p2 > 1.0f) p2 = 1.0f;

        float p_max = (p1 > p2) ? p1 : p2;
        float diff = fabsf(p1 - p2);

        if (p_max > APPS_MIN_TRAVEL_FOR_CHECK) {
            if (diff > APPS_MAX_DIFF_ALLOWED) {
                if (implaus_counter < 255) implaus_counter++;
            } else {
                implaus_counter = 0;
            }
        } else {
            implaus_counter = 0;
        }

        if (implaus_counter >= APPS_IMPLAUS_COUNT) apps_implaus = true;

        if (apps_implaus && p1 < 0.05f && p2 < 0.05f) {
            apps_implaus = false;
            implaus_counter = 0;
        }

        float pedal = 0.0f;
        if (!apps_implaus) pedal = 0.5f * (p1 + p2);

        if (pedal < 0.0f) pedal = 0.0f;
        if (pedal > 1.0f) pedal = 1.0f;

        pedal_filt += PEDAL_FILTER_ALPHA * (pedal - pedal_filt);

        bool brake_active = false;

        if (brake_active && pedal_filt > 0.25f) brake_latched = true;

        if (brake_latched && pedal_filt < 0.05f) brake_latched = false;

        if (apps_implaus || brake_latched)
            tq_cmd = 0.0f;
        else
            tq_cmd = pedal_filt * TORQUE_MAX_NM;

        int p1_i = (int)(p1 * 1000.0f);
        int p2_i = (int)(p2 * 1000.0f);
        int pf_i = (int)(pedal_filt * 1000.0f);
        int tq_i = (int)(tq_cmd * 100.0f);

        log_printf(LOG_INFO,
            "APP1=%u (%d.%03d)  APP2=%u (%d.%03d)  ped_f=%d.%03d  tq=%d.%02d Nm  impl=%d  brake=%d\r\n",
            raw1, p1_i/1000, p1_i%1000,
            raw2, p2_i/1000, p2_i%1000,
            pf_i/1000, pf_i%1000,
            tq_i/100, tq_i%100,
            apps_implaus ? 1 : 0,
            brake_latched ? 1 : 0);

        osDelay(pdMS_TO_TICKS(TORQUE_TASK_PERIOD_MS));
    }

    /* USER CODE END StartTorqueTask */
}

/* USER CODE BEGIN Application */
/* USER CODE END Application */
