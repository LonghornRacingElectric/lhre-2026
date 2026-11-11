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
#include "cmsis_os.h"
#include "main.h"
#include "task.h"

#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "tim.h"
#include "usbd_cdc_if.h"

#include "longhorn/rtos/dfu.h"
#include "longhorn/rtos/led.h"
#include "longhorn/rtos/logger.h"
#include "longhorn/rtos/usb.h"
#include "longhorn/usb_base.h"
#include "ota/ota_flash.h"

#include "imu.h"
#include "spi.h"
#include "usm_can.h"
#include "wheel_speed.h"
#include <math.h>
#include <string.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* Alternate speed/IMU and Hall diagnostics: each row appears at 10 Hz. */
#define USM_DEBUG_SLOT_MS 50u

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
extern USBD_HandleTypeDef hUsbDeviceFS;
static dfu_config dfu_conf = {
    .delay_fn = (Delay_fn)osDelay,
    .gpiox = GPIOB,
    .pin = GPIO_PIN_9,
    .pin_set_fn = (PinSet_fn)HAL_GPIO_WritePin,
    .reset_fn = (SystemReset_fn)HAL_NVIC_SystemReset,
    .set_bank1_fn = (SetBank1_fn)ota_set_bank1,
};

static rainbow_led_t led_conf = {
    .ccr2 = &TIM2->CCR1,
    .ccr1 = &TIM2->CCR2,
    .ccr3 = &TIM2->CCR3,
    .channel1 = TIM_CHANNEL_1,
    .channel2 = TIM_CHANNEL_2,
    .channel3 = TIM_CHANNEL_3,
    .pwm_start = (HAL_PWM_Start_Fn)HAL_TIM_PWM_Start,
    .timer_handle = &htim2,
};

osThreadId_t wheelSpeedTaskHandle;
const osThreadAttr_t wheelSpeedTask_attributes = {
    .name = "wheelSpeedTask",
    .priority = (osPriority_t)osPriorityNormal,
    .stack_size = 512 * 8};
/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
    .name = "defaultTask",
    .priority = (osPriority_t)osPriorityNormal,
    .stack_size = 512 * 4};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
osThreadId_t ledHandle;

static uint8_t USM_DebugTransmit(uint8_t *data, uint16_t length);
void StartWheelSpeedTask(void *argument);
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
  defaultTaskHandle =
      osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  led_init(&led_conf);
  ledHandle = led_start_thread();

  wheelSpeedTaskHandle =
      osThreadNew(StartWheelSpeedTask, NULL, &wheelSpeedTask_attributes);
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
void StartDefaultTask(void *argument) {
  /* init code for USB_Device */
  MX_USB_Device_Init();
  /* USER CODE BEGIN StartDefaultTask */
  if (init_logging(USM_DebugTransmit) == -1) {
    osThreadTerminate(ledHandle);
  }

  init_dfu(dfu_conf);
  dfu_start_thread();

  usm_can_init();

  /* Infinite loop */
  for (;;) {
    usm_can_debug();
    osDelay(1000);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
/* USB retains the transmit pointer until completion. Keep a private copy so
 * another queued log cannot overwrite a transfer already in flight. Called
 * only by the logger task; disconnected/busy USB drops logs without waiting. */
static uint8_t USM_DebugTransmit(uint8_t *data, uint16_t length) {
  static uint8_t tx_buffer[256];
  USBD_CDC_HandleTypeDef *cdc = hUsbDeviceFS.pClassData;
  if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED || cdc == NULL)
    return USBD_FAIL;
  if (cdc->TxState != 0u) return USBD_BUSY;
  if (length > sizeof(tx_buffer)) return USBD_FAIL;
  memcpy(tx_buffer, data, length);
  return CDC_Transmit_FS(tx_buffer, length);
}

void StartWheelSpeedTask(void *argument) {
  osDelay(2000); // wait for USB CDC to enumerate
  WheelSpeed_Init(&hspi2);
  int imu_status = IMU_Init(&hspi2);
  imu_data_t imu = {0};
  uint32_t next_tick = osKernelGetTickCount();
  uint32_t last_debug_tick = next_tick;
  const uint32_t debug_ticks =
      (osKernelGetTickFreq() * USM_DEBUG_SLOT_MS + 999u) / 1000u;
  bool hall_debug_row = false;
  for (;;) {
    WheelSpeed_Update();

    float speed = WheelSpeed_GetSpeed();
    usm_can_update_wheel_speed(speed);
    usm_can_update_phase(WheelSpeed_GetPhaseEstimate(), WheelSpeed_GetPhaseSequence());

    int imu_read_status = imu_status == IMU_OK ? IMU_Read(&imu) : imu_status;
    if (imu_read_status == IMU_OK) {
      usm_can_update_accel(imu.accel_x, imu.accel_y, imu.accel_z);
    }

    uint32_t debug_now = osKernelGetTickCount();
    if ((uint32_t)(debug_now - last_debug_tick) >= debug_ticks) {
      last_debug_tick = debug_now;
      const wheel_phase_output_t *phase = WheelSpeed_GetPhaseEstimate();
      /* One bounded queue write per slot, never a burst of USB messages.
       * The logger adds its own timestamp and CRLF. Invalid data prints nan. */
      if (!hall_debug_row) {
        float phase_speed = phase->valid ? phase->speed_rad_s : NAN;
        bool imu_ok = imu_read_status == IMU_OK;
        log_printf(LOG_INFO,
                   "USM ws=%.6g rpm=%.6g valid=%u status=0x%03lx legacy=%.6g "
                   "imu_init=%d imu_read=%d ax=%.6g ay=%.6g az=%.6g",
                   (double)phase_speed, (double)(phase_speed * 9.5492966f),
                   (unsigned)phase->valid, (unsigned long)phase->status,
                   (double)speed, imu_status, imu_read_status,
                   (double)(imu_ok ? imu.accel_x : NAN),
                   (double)(imu_ok ? imu.accel_y : NAN),
                   (double)(imu_ok ? imu.accel_z : NAN));
      } else {
        const wheel_phase_sample_t *sample = WheelSpeed_GetPhaseSample();
        log_printf(LOG_INFO,
                   "HALL init=0x%x fresh=0x%x r0=%.6g r1=%.6g r2=%.6g r3=%.6g "
                   "phase_valid=%u phase=%.6g pair_err=%.6g innov=%.6g q=%.3g",
                   (unsigned)WheelSpeed_GetInitializedMask(),
                   (unsigned)sample->fresh_mask,
                   (double)sample->radial[0], (double)sample->radial[1],
                   (double)sample->radial[2], (double)sample->radial[3],
                   (unsigned)phase->phase_valid,
                   (double)phase->electrical_phase_rad,
                   (double)phase->pair_disagreement_rad,
                   (double)phase->innovation_rad, (double)phase->quality);
      }
      hall_debug_row = !hall_debug_row;
    }
    next_tick += 1u;
    uint32_t now_tick = osKernelGetTickCount();
    if ((int32_t)(next_tick - now_tick) <= 0) next_tick = now_tick + 1u;
    osDelayUntil(next_tick);
  }
}
/* USER CODE END Application */
