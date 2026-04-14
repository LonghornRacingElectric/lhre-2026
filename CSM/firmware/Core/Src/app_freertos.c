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
//#include "led_base.h"
#include "longhorn/led_base.h"
#include "tim.h"
#include "ride_height.h"
#include "usbd_cdc_if.h"
#include "platform/argus_timer.h"
#include "platform/argus_s2pi.h"
#include "platform/board_config.h"
#include "adc.h"
#include "spi.h"
#include "suspot.h"
#include "strain_gauge.h"
#include "imu.h"
#include "longhorn/rtos/dfu.h"
#include "longhorn/rtos/logger.h"
#include "longhorn/rtos/usb.h"
#include "longhorn/usb_base.h"
#include "ota/ota_flash.h"
#include "csm_can.h"
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
osThreadId_t IMUTaskHandle;
const osThreadAttr_t IMUTask_attributes = {
    .name = "IMUTask",
    .priority = (osPriority_t) osPriorityNormal,
    .stack_size = 2048 * 4
};

// osThreadId_t rideHeightTaskHandle;
// const osThreadAttr_t rideHeightTask_attributes = {
//     .name = "rideHeightTask",
//     .priority = (osPriority_t) osPriorityNormal,
//     .stack_size = 2048 * 4
// };

osThreadId_t susPotTaskHandle;
const osThreadAttr_t susPotTask_attributes = {
    .name = "susPotTask",
    .priority = (osPriority_t) osPriorityNormal,
    .stack_size = 2048 * 4
};
osThreadId_t strainGaugeTaskHandle;
const osThreadAttr_t strainGaugeTask_attributes = {
    .name = "strainGaugeTask",
    .priority = (osPriority_t) osPriorityNormal,
    .stack_size = 2048 * 4
};
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
void StartIMUTask(void *argument);
// void StartRideHeightTask(void *argument);
void StartSusPotTask(void *argument);
void StartStrainGaugeTask(void *argument);
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

    IMUTaskHandle = osThreadNew(StartIMUTask, NULL, &IMUTask_attributes);
    susPotTaskHandle = osThreadNew(StartSusPotTask, NULL, &susPotTask_attributes);
    // rideHeightTaskHandle = osThreadNew(StartRideHeightTask, NULL, &rideHeightTask_attributes);
    // strainGaugeTaskHandle = osThreadNew(StartStrainGaugeTask, NULL, &strainGaugeTask_attributes);
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
    /* Infinite loop */
    osDelay(500);

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
    for (;;) {
        osDelay(1000);
    }







  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
void StartIMUTask(void *argument)
{
    osDelay(5000);

    int result = IMU_Init(&hspi1);
    if (result != 0) {
        // WHO_AM_I failed -- print result for debugging
        osDelay(500);  // give USB time to be ready
        char buf[64];
        snprintf(buf, sizeof(buf), "IMU init failed, WHO_AM_I=0x%02X\r\n", result);
        CDC_Transmit_FS((uint8_t*)buf, strlen(buf));
        osDelay(500);
        vTaskDelete(NULL);  // kill task if IMU not found
        return;
    }

    CDC_Transmit_FS((uint8_t*)"IMU init success\r\n", 18);
    osDelay(500);
    uint8_t whoami = imu_read(0x0F);
    uint8_t ctrl1  = imu_read(0x10);
    uint8_t ctrl2  = imu_read(0x11);
    char dbg[128];
    snprintf(dbg, sizeof(dbg), "WHO_AM_I=0x%02X CTRL1_XL=0x%02X CTRL2_G=0x%02X\r\n", whoami, ctrl1, ctrl2);
    CDC_Transmit_FS((uint8_t*)dbg, strlen(dbg));
    osDelay(500);

    imu_data_t data;
    char buf[128];

    for (;;)
    {
        IMU_GetData(&data);
        int len = snprintf(buf, sizeof(buf),
            "Accel: %.3f %.3f %.3f m/s^2 | Gyro: %.3f %.3f %.3f rad/s\r\n",
            data.accel.x, data.accel.y, data.accel.z,
            data.gyro.x,  data.gyro.y,  data.gyro.z);
        if (len > 0)
            CDC_Transmit_FS((uint8_t*)buf, (uint16_t)len);
        osDelay(100);  // 10 Hz print rate
    }
}

void StartStrainGaugeTask(void *argument) {
  osDelay(100);
  int32_t value;

  for(;;) {
    value = strainGaugeGetVal(&hadc1, ADC_CHANNEL_4);

    char buf[64];
    int len = snprintf(buf, sizeof(buf), "STRAIN_GAUGE Reading: %ld\r\n", value);
    if (len > 0) {
      CDC_Transmit_FS((uint8_t*)buf, (uint16_t)len);
    }

    osDelay(1000); // 1 Hz sampling
  }
}
void StartSusPotTask(void *argument)
{
    osDelay(100);
    uint32_t value;

    for (;;)
    {
        value = susPotGetVal(&hadc2, ADC_CHANNEL_12);
        float percent = ((float)value / 4095.0f) * 100.0f;
        float inches = (percent / 100.0f) * 0.04f; // Assuming 0.04 inches per percent
        char buf[64];
        int len = snprintf(buf, sizeof(buf), "SUSPOT Travel: %.2f\r\n", inches);
        // int len = snprintf(buf, sizeof(buf), "SUSPOT Travel: %.2f%%\r\n", percent);
        // int len = snprintf(buf, sizeof(buf), "ADC output: %lu\r\n", value);

        if (len > 0) {
          CDC_Transmit_FS((uint8_t*)buf, (uint16_t)len);
          csm_can_update_strain_gauge_sus_pot(inches, 0.0f); // TODO: pass actual strain voltage
        }

        osDelay(1000); // 1 Hz sampling
    }
}

// void StartRideHeightTask(void *argument)
// {
//     osDelay(10000);

//     ride_height_init();

//     char buf[64];
//     for (;;)
//     {
//         float distance_mm = ride_height_get_distance_mm();
//         int len = snprintf(buf, sizeof(buf), "Distance: %.2f mm\r\n", distance_mm);
//         if (len > 0)
//         {
//             CDC_Transmit_FS((uint8_t *)buf, (uint16_t)len);
//         }
//         osDelay(100);
//     }
// }
// void StartRideHeightTask(void *argument)
// {
//     osDelay(3000);

//     ride_height_init();

//     for (;;)
//     {
//         CDC_Transmit_FS((uint8_t *)"heartbeat\r\n", 11);
//         osDelay(1000);
//     }
// }
/* USER CODE END Application */

