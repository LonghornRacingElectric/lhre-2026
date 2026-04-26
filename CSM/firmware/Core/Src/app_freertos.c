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
#include "spi_mutex.h"
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
osThreadId_t accelRideHeightTaskHandle;
const osThreadAttr_t accelRideHeightTask_attributes = {
    .name = "accelRideHeightTask",
    .priority = (osPriority_t) osPriorityNormal,
    .stack_size = 4096 * 4
};

osThreadId_t susPotStrainGaugeTaskHandle;
const osThreadAttr_t susPotStrainGaugeTask_attributes = {
    .name = "susPotStrainGaugeTask",
    .priority = (osPriority_t) osPriorityNormal,
    .stack_size = 2048 * 4
};
osThreadId_t IMUTaskHandle;
const osThreadAttr_t IMUTask_attributes = {
    .name = "IMUTask",
    .priority = (osPriority_t) osPriorityNormal,
    .stack_size = 2048 * 4
};

osThreadId_t rideHeightTaskHandle;
const osThreadAttr_t rideHeightTask_attributes = {
    .name = "rideHeightTask",
    .priority = (osPriority_t) osPriorityNormal,
    .stack_size = 4096 * 4
};

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
void StartAccelRideHeightTask(void *argument);
void StartSusPotStrainGaugeTask(void *argument);
void StartIMUTask(void *argument);
void StartRideHeightTask(void *argument);
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

    accelRideHeightTaskHandle = osThreadNew(StartAccelRideHeightTask, NULL, &accelRideHeightTask_attributes);
    susPotStrainGaugeTaskHandle = osThreadNew(StartSusPotStrainGaugeTask, NULL, &susPotStrainGaugeTask_attributes);
    // IMUTaskHandle = osThreadNew(StartIMUTask, NULL, &IMUTask_attributes);
    // susPotTaskHandle = osThreadNew(StartSusPotTask, NULL, &susPotTask_attributes);
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
    spi_mutex_init();
    csm_can_init();

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
void StartAccelRideHeightTask(void *argument)
{
    // Wait for USB and CAN to be ready
    osDelay(5000);

    CDC_Transmit_FS((uint8_t*)"Accel+RH task started\r\n", 23);
    osDelay(500);

    // Init ride height sensor
    ride_height_init();
    osDelay(500);

    // Init IMU
    int result = IMU_Init(&hspi1);
    if (result != 0) {
        char buf[64];
        snprintf(buf, sizeof(buf), "IMU init failed, WHO_AM_I=0x%02X\r\n", result);
        CDC_Transmit_FS((uint8_t*)buf, strlen(buf));
        // continue anyway — ride height will still work
    } else {
        CDC_Transmit_FS((uint8_t*)"IMU init success\r\n", 18);
    }

    // Stop rainbow LED and use it for ride height indication
    led_disable();

    imu_data_t imu_data = {0};
    char buf[128];

    for (;;)
{
    float distance_mm = ride_height_get_distance_mm();
    uint8_t quality = ride_height_get_quality();

    // Non-blocking IMU read — only update if data is ready
    if (result == 0 && IMU_AccelStatus() && IMU_GyroStatus()) {
        IMU_GetAccel(&imu_data.accel);
        IMU_GetGyro(&imu_data.gyro);
    }

    csm_can_update_accel_ride_height(
        imu_data.accel.x,
        imu_data.accel.y,
        imu_data.accel.z,
        distance_mm
    );

    // LED update
    float t = (distance_mm - 20.0f) / (200.0f - 20.0f);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    led_set((1.0f - t) * 0.5f, 0.0f, t * 0.5f);

    // Debug print
    int len = snprintf(buf, sizeof(buf),
        "RH: %.2f mm Q:%d | Accel: %.3f %.3f %.3f m/s^2\r\n",
        distance_mm, quality,
        imu_data.accel.x, imu_data.accel.y, imu_data.accel.z);
    if (len > 0)
        CDC_Transmit_FS((uint8_t*)buf, (uint16_t)len);

    osDelay(10);
}
}

void StartSusPotStrainGaugeTask(void *argument)
{
    osDelay(1000);  // wait for USB and CAN to be ready

    char buf[64];

    for (;;)
    {
        // Read sus pot
        uint32_t sus_pot_raw = susPotGetVal(&hadc2, ADC_CHANNEL_12);
        float sus_pot_voltage = ((float)sus_pot_raw / 4095.0f) * 5000.0f;

        // Read strain gauge
        int32_t strain_raw = strainGaugeGetVal(&hadc1, ADC_CHANNEL_4);
        float strain_voltage = ((float)strain_raw / 4095.0f) * 5000.0f;

        // Send CAN packet
        csm_can_update_strain_gauge_sus_pot(sus_pot_voltage, strain_voltage);

        // Debug print
        int len = snprintf(buf, sizeof(buf),
            "SusPot: %.2f mV | Strain: %.2f mV\r\n",
            sus_pot_voltage, strain_voltage);
        if (len > 0)
            CDC_Transmit_FS((uint8_t*)buf, (uint16_t)len);

        osDelay(10);  // 100Hz
    }
}
void StartIMUTask(void *argument)
{
    osDelay(8000);

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
        float voltage = ((float)value / 4095.0f) * 5000.0f; //adjusted for telem scaling
        // float inches = (percent / 100.0f) * 0.04f; // Assuming 0.04 inches per percent
        char buf[64];
        int len = snprintf(buf, sizeof(buf), "SUSPOT Travel: %.2f\r\n", voltage);
        // int len = snprintf(buf, sizeof(buf), "SUSPOT Travel: %.2f%%\r\n", percent);
        // int len = snprintf(buf, sizeof(buf), "ADC output: %lu\r\n", value);

        if (len > 0) {
          CDC_Transmit_FS((uint8_t*)buf, (uint16_t)len);
          csm_can_update_strain_gauge_sus_pot(voltage, 0.0f); // TODO: pass actual strain voltage
        }

        osDelay(1000); // 333 Hz sampling
    }
}

// void StartCANTask(void *argument) {
//   osDelay(500);
//   csm_can_update_strain_gauge_sus_pot(0.0f, 0.0f); // TODO: pass actual strain voltage and suspot travel
// }

void StartRideHeightTask(void *argument)
{
    osDelay(5000);
    CDC_Transmit_FS((uint8_t*)"RH task started\r\n", 17);
    osDelay(500);
    ride_height_init();

    led_disable(); // stops the rainbow thread



    char buf[64];
    for (;;)
    {
        float distance_mm = ride_height_get_distance_mm();
        uint8_t quality = ride_height_get_quality();

        // maps distance from red to blue, <20.0mm is full red, >100.0mm is full blue, with linear in between
        float t = (distance_mm - 20.0f) / (200.0f - 20.0f);
        if (t < 0.0f) {
          t = 0.0f;
        }
        if (t > 1.0f) {
          t = 1.0f;
        }
        float red = (1.0f - t) * 0.5f;
        float blue = t * 0.5f;
        led_set(red, 0.0f, blue);

        int len = snprintf(buf, sizeof(buf), "Distance: %.2f mm  Quality: %d\r\n", distance_mm, quality);
        if (len > 0)
            CDC_Transmit_FS((uint8_t*)buf, (uint16_t)len);
        osDelay(10);
    }
}