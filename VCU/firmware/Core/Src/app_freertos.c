/* USER CODE BEGIN Header */
/**
 *******************************************************************************
 * File Name          : app_freertos.c
 * Description        : Code for FreeRTOS applications (VCU)
 *******************************************************************************
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

#include "adc.h"
#include "tim.h"
#include "usbd_cdc_if.h"

#include "longhorn/rtos/dfu.h"
#include "longhorn/rtos/led.h"
#include "longhorn/rtos/logger.h"
#include "longhorn/rtos/usb.h"
#include "longhorn/usb_base.h"

#include "vcu_model/inc/vcu_inputs.h"
#include "vcu_model/inc/vcu_model.h"
#include "vcu_model/inc/vcu_outputs.h"

#include "vcu_can.h"

#include <stdint.h>
#include <stdlib.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
// External ADC handles
extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern ADC_HandleTypeDef hadc3;

// DMA buffers
// APPS1, APPS2 from ADC3 (configured in adc.c)
extern volatile uint16_t adc3_dma_buf[2];
// BSE from ADC2
volatile uint16_t adc2_dma_buf[2];

// Thread handles
osThreadId_t systemTaskHandle;
osThreadId_t controlTaskHandle;
osThreadId_t ledHandle;

// Thread attributes
const osThreadAttr_t systemTask_attributes = {
    .name = "SystemTask",
    .priority = osPriorityNormal,
    .stack_size = 512 * 4,
};

const osThreadAttr_t controlTask_attributes = {
    .name = "ControlTask",
    .priority = osPriorityAboveNormal,
    .stack_size = 2048,
};
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

static vcu_parameters_t s_params = {
    .apps =
        {
            .apps1_min_adc_v = 782u,
            .apps1_max_adc_v = 3262u,
            .apps2_min_adc_v = 382u,
            .apps2_max_adc_v = 1586u,
            .implaus_debounce_time_ms = 100u,
            .max_allowable_diff = 0.10f,
            .min_travel_threshold = 0.10f,
            .max_travel_restore_threshold = 0.05f,
        },
    .torque_map =
        {
            .max_torque_nm = 200.0f,
        },

    .bse =
        {
            .bse_off_psi = 30.0f,
            .bse_on_psi = 50.0f,
            .bse_adc_at_min_psi_v = 156u,
            .bse_adc_at_max_psi_v = 635u,
            .bse_max_psi = 1000.0f,
            .max_pedal_while_braking = 0.25f,
            .max_pedal_restore_threshold = 0.05f,
        },
    .buzzer_duration_ms = 1800u,
    .brake_enable_threshold = 0.1f,
};

static vcu_model_context_t ctx = {0};

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
    .name = "defaultTask",
    .priority = (osPriority_t)osPriorityNormal,
    .stack_size = 128 * 4};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

// Function prototypes
void StartSystemTask(void *argument);
void StartControlTask(void *argument);

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

  // Create system/init task
  systemTaskHandle = osThreadNew(StartSystemTask, NULL, &systemTask_attributes);

  // Configure and start rainbow LED thread
  rainbow_led_t led = (rainbow_led_t){
      .ccr1 = &TIM2->CCR2,
      .ccr2 = &TIM2->CCR1,
      .ccr3 = &TIM2->CCR3,
      .channel1 = TIM_CHANNEL_1,
      .channel2 = TIM_CHANNEL_2,
      .channel3 = TIM_CHANNEL_3,
      .pwm_start = (HAL_PWM_Start_Fn)HAL_TIM_PWM_Start,
      .timer_handle = &htim2,
  };
  led_init(&led);
  ledHandle = led_start_thread();

  // Create main control task
  controlTaskHandle =
      osThreadNew(StartControlTask, NULL, &controlTask_attributes);

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

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
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
  /* Infinite loop */
  for (;;) {
    osDelay(1);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
// SystemTask: one-time initialization (USB, logging, DFU, ADC DMA, CAN) -----
void StartSystemTask(void *argument) {
  // USB + logging
  MX_USB_Device_Init();

  if (init_logging(CDC_Transmit_FS) == -1) {
    // If USB logging fails, stop LED thread so we notice
    osThreadTerminate(ledHandle);
  }

  // DFU (bootloader) config
  dfu_config dfu = {
      .delay_fn = (Delay_fn)osDelay,
      .gpiox = GPIOB,
      .pin = GPIO_PIN_7,
      .pin_set_fn = (PinSet_fn)HAL_GPIO_WritePin,
      .reset_fn = (SystemReset_fn)HAL_NVIC_SystemReset,
  };
  init_dfu(dfu);
  dfu_start_thread();

  // Start DMA conversions for APPS (ADC3) and BSE (ADC2) once
  if (HAL_ADC_Start_DMA(&hadc3, (uint32_t *)adc3_dma_buf, 2) != HAL_OK) {
    Error_Handler();
  }
  if (HAL_ADC_Start_DMA(&hadc2, (uint32_t *)adc2_dma_buf, 2) != HAL_OK) {
    Error_Handler();
  }

  // Initialize CAN (longhorn-lib, inverter torque command + feedback)
  vcu_can_init();

  // Nothing else to do here; keep the task alive for future use
  for (;;) {
    osDelay(pdMS_TO_TICKS(500));
  }
}

// ControlTask: main 10ms loop (ADC -> model -> CAN -> logging) --------------
void StartControlTask(void *argument) {
  // Let system init (USB, DFU, CAN, DMA) finish
  osDelay(pdMS_TO_TICKS(200));

  static uint32_t last_tick = 0;
  last_tick = osKernelGetTickCount();

  vcu_inputs_t in = {0};
  vcu_outputs_t out = {0};

  vcu_model_init(&ctx, &s_params);

  uint32_t log_div = 0;  // for slower logging
  uint32_t adc1_val = 0; // optional polled ADC1 read

  for (;;) {
    uint32_t current_tick = osKernelGetTickCount();
    uint32_t dt_ms = current_tick - last_tick;
    last_tick = current_tick;

    // Make sure DMA conversions are running for APPS/BSE.
    // If DMA is already running, HAL will usually return HAL_BUSY; that's OK.
    HAL_StatusTypeDef s;

    s = HAL_ADC_Start_DMA(&hadc3, (uint32_t *)adc3_dma_buf, 2);
    if (s != HAL_OK && s != HAL_BUSY) {
      Error_Handler();
    }

    s = HAL_ADC_Start_DMA(&hadc2, (uint32_t *)adc2_dma_buf, 2);
    if (s != HAL_OK && s != HAL_BUSY) {
      Error_Handler();
    }

    // Optional: small delay to let conversions complete before we read
    osDelay(1);

    // To-Do: Steering
    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
      adc1_val = HAL_ADC_GetValue(&hadc1);
    }
    HAL_ADC_Stop(&hadc1);

    // Read pedal sensors from DMA buffers
    in.apps1_raw = adc3_dma_buf[0];
    in.apps2_raw = adc3_dma_buf[1];
    in.bse_raw = adc2_dma_buf[0];

    // TODO: update based on CAN packets
    in.contactors_closed = false;
    in.drive_switch = false;

    // Run control model
    vcu_model_step(&ctx, &in, &out, dt_ms);

    // Send torque command to inverter (Nm)
    vcu_can_set_torque(out.torque_cmd);

    // Read torque command
    vcu_can_read_feedback();

    // Read contactor status
    vcu_can_read_contactor_status();

    // Throttle logging: every 20 loops => ~200 ms at 10 ms loop
    if (++log_div >= 20) {
      log_div = 0;

      int p1_i = (int)(out.apps1_travel * 1000.0f);
      int p2_i = (int)(out.apps2_travel * 1000.0f);
      int pf_i = (int)(out.pedal_filtered * 1000.0f);
      int tq_i = (int)(out.torque_cmd * 100.0f);
      int psi_i = (int)(out.bse_psi);

      int inv_fb_tq = (int)(inverter_torque_fb * 100.0f);
      int inv_bus_v = (int)(inverter_bus_voltage * 10.0f);

      log_printf(LOG_INFO,
                 "ADC1=%u  "
                 "APP1=%u (%d.%03d)  "
                 "APP2=%u (%d.%03d)  "
                 "BSE=%u (%d psi)  "
                 "ped_f=%d.%03d  "
                 "tq_cmd=%d.%02d Nm  "
                 "impl=%d  brake_act=%d  brake_lat=%d  "
                 "HV_Cont=%d  HVC_St=%d  "
                 "INV: fb_tq=%d.%02d Nm  rpm=%d  bus=%d.%d V\r\n",
                 adc1_val, in.apps1_raw, p1_i / 1000, p1_i % 1000, in.apps2_raw,
                 p2_i / 1000, p2_i % 1000, in.bse_raw, psi_i, pf_i / 1000,
                 pf_i % 1000, tq_i / 100, tq_i % 100,
                 out.faults.apps_implaus ? 1 : 0, out.brake_active ? 1 : 0,
                 out.faults.brake_latched ? 1 : 0, hv_contactors_closed ? 1 : 0,
                 hvc_state, inv_fb_tq / 100, inv_fb_tq % 100, inverter_rpm,
                 inv_bus_v / 10, inv_bus_v % 10);
    }

    // 10 ms control loop (100 Hz)
    osDelay(pdMS_TO_TICKS(10));
  }
}

/* USER CODE END Application */
