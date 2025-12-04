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

extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern ADC_HandleTypeDef hadc3;
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
#define APPS_MIN_ADC      815 // arbitrary pedal tuning
#define APPS_MAX_ADC      3390
#define MAX_TORQUE_NM     80.0f
#define TORQUE_ALPHA      0.5f // new_value = old_value + smoothing * (new_input - old_value)
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
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 128 * 4
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
/* (already declared above) */
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

    /* create additional tasks */
    defaultTask2Handle = osThreadNew(StartDefaultTask2, NULL, NULL);
    adcTaskHandle = osThreadNew(StartADCTask, NULL, &adcTask_attributes);
    torqueTaskHandle = osThreadNew(StartTorqueTask, NULL, &torqueTask_attributes);
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
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

