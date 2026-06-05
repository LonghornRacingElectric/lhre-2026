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
#include "ota/ota_flash.h"

#include "vcu_model/inc/vcu_inputs.h"
#include "vcu_model/inc/vcu_model.h"
#include "vcu_model/inc/vcu_outputs.h"

#include "vcu_can.h"

#include "params/acceleration_params.h"
#include "params/autocross_params.h"
#include "params/endurance_params.h"
#include "params/skidpad_params.h"

#include <stdint.h>
#include <stdlib.h>
#include <math.h>
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
extern volatile uint16_t adc2_dma_buf[2];

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
#define SELECTED_PARAMS autocross_params
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define ADC_APPS_SCALE_V 3.3f
#define ADC_STEERING_SCALE_V 3.3f
#define STEERING_DIVIDER_R_TOP_OHMS 5100.0f
#define STEERING_DIVIDER_R_BOTTOM_OHMS 10000.0f
#define STEERING_SENSOR_SUPPLY_V 4.64f
#define STEERING_SENSOR_MIN_RATIO 0.10f
#define STEERING_SENSOR_MAX_RATIO 0.90f
#define STEERING_SENSOR_ANGLE_RANGE_DEG 360.0f
#define STEERING_SENSOR_ANGLE_OFFSET_DEG 0.0f
#define CONTROL_LOOP_PERIOD_MS 3u

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

static vcu_parameters_t s_params;
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
static float steering_adc_to_sensor_voltage(float adc_voltage_v);
static float steering_sensor_voltage_to_percent(float sensor_voltage_v);
static float steering_sensor_voltage_to_angle_deg(float sensor_voltage_v);

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
  /* creation of defaultTask */
  defaultTaskHandle =
      osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

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
    osDelay(10000);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
static float steering_adc_to_sensor_voltage(float adc_voltage_v) {
  const float divider_gain =
      (STEERING_DIVIDER_R_TOP_OHMS + STEERING_DIVIDER_R_BOTTOM_OHMS) /
      STEERING_DIVIDER_R_BOTTOM_OHMS;
  return adc_voltage_v * divider_gain;
}

// 48% (rightmost) to 114% (leftmost)

static float steering_sensor_voltage_to_angle_deg(float sensor_voltage_v) {
  // Assumes the PIHER sensor output is ratiometric from 10% to 90% of the
  // 4.64 V supply across a 360 degree sweep.
  float angle_pct = steering_sensor_voltage_to_percent(sensor_voltage_v);
  float raw_angle_deg = (angle_pct * STEERING_SENSOR_ANGLE_RANGE_DEG) -
         (0.5f * STEERING_SENSOR_ANGLE_RANGE_DEG);
  float adjusted_angle_deg = raw_angle_deg + STEERING_SENSOR_ANGLE_OFFSET_DEG;
  return adjusted_angle_deg;
}

static float steering_sensor_voltage_to_percent(float sensor_voltage_v) {
  const float sensor_min_v =
      STEERING_SENSOR_SUPPLY_V * STEERING_SENSOR_MIN_RATIO;
  const float sensor_max_v =
      STEERING_SENSOR_SUPPLY_V * STEERING_SENSOR_MAX_RATIO;
  const float clamped_sensor_v =
      fminf(fmaxf(sensor_voltage_v, sensor_min_v), sensor_max_v);
  float pct = (clamped_sensor_v - sensor_min_v) / (sensor_max_v - sensor_min_v);
  if(pct > 0.8f) {
    pct -= 1.0f;
  }
  return pct;
}

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
      .set_bank1_fn = (SetBank1_fn)ota_set_bank1,
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
  osDelay(pdMS_TO_TICKS(1000));

  vcu_can_clear_inverter_faults();

  static uint32_t last_tick = 0;
  last_tick = osKernelGetTickCount();

  vcu_inputs_t in = {0};
  vcu_outputs_t out = {0};

  s_params = SELECTED_PARAMS;
  vcu_model_init(&ctx, &s_params);

  uint32_t adc1_val = 0; // steering ADC1 read
  float steering_adc_voltage_v = 0.0f;
  float steering_sensor_voltage_v = 0.0f;
  float steering_angle_pct = 0.0f;
  float steering_angle_deg = 0.0f;

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
    // osDelay(1);

    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
      adc1_val = HAL_ADC_GetValue(&hadc1);
    }
    HAL_ADC_Stop(&hadc1);

    steering_adc_voltage_v =
        ((float)adc1_val * ADC_STEERING_SCALE_V) / ADC_MAX_VAL;
    steering_sensor_voltage_v =
        steering_adc_to_sensor_voltage(steering_adc_voltage_v);
    steering_angle_pct =
        steering_sensor_voltage_to_percent(steering_sensor_voltage_v);
    // steering_angle_deg =
    //     steering_sensor_voltage_to_angle_deg(steering_sensor_voltage_v);
    steering_angle_deg = ((steering_angle_pct - 0.02f) / 0.62f - 0.5f) * 230.0f + 7.0f;  // TODO chud temp tuning

    // Read pedal sensors from DMA buffers
    in.apps1_raw = ((float)adc3_dma_buf[1] * ADC_APPS_SCALE_V) / ADC_MAX_VAL;
    in.apps2_raw = ((float)adc3_dma_buf[0] * ADC_APPS_SCALE_V) / ADC_MAX_VAL;
    in.bse1_raw = ((float)adc2_dma_buf[0] * ADC_BSE_SCALE_V) / ADC_MAX_VAL;
    in.bse2_raw = ((float)adc2_dma_buf[1] * ADC_BSE_SCALE_V) / ADC_MAX_VAL;

    in.drive_switch = is_drive_switch_pressed();

    in.contactors_closed = hvc_tractive_ready();
    inverter_voltages_t inverter_voltages = vcu_can_get_inverter_voltages();
    inverter_currents_t inverter_currents = vcu_can_get_inverter_currents();
    vcu_battery_pack_status_t pack_status = vcu_can_get_battery_pack_status();

    in.motor_speed_rpm = fabsf(vcu_can_get_motor_speed_rpm());
    in.motor_speed_valid = vcu_can_is_motor_speed_valid();
    in.min_cell_voltage_v = vcu_can_get_min_cell_voltage_v();
    in.max_cell_voltage_v = vcu_can_get_max_cell_voltage_v();
    if (in.min_cell_voltage_v <= 0.0f) {
      in.min_cell_voltage_v = s_params.torque_map.low_cell_derate_start_v;
    }
    if (in.max_cell_voltage_v <= 0.0f) {
      in.max_cell_voltage_v = in.min_cell_voltage_v;
    }
    in.battery_voltage_v = inverter_voltages.dc_bus;
    in.battery_current_a = inverter_currents.dc_bus;
    in.battery_soc_pct = pack_status.state_of_charge_pct;
    in.min_cell_temp_c = pack_status.min_cell_temp_c;
    in.max_cell_temp_c = pack_status.max_cell_temp_c;
    in.inverter_voltage_valid = vcu_can_is_inverter_voltage_valid();
    in.inverter_current_valid = vcu_can_is_inverter_current_valid();
    in.battery_pack_status_valid = pack_status.valid;

    // Run control model
    vcu_model_step(&ctx, &in, &out, dt_ms);

    vcu_can_set_model_inputs(&in);
    vcu_can_set_model_outputs(&out);
    vcu_can_set_steering_angle_deg(steering_angle_deg);


    
    // log_printf(LOG_INFO,
    //            "\nAPPS1_RAW:%.3f APPS1_PCT:%.3f\nAPPS2_RAW:%.3f "
    //            "APPS2_PCT:%.3f\nAPPS: %.3f\n\n",
    //            (double)in.apps1_raw, (double)out.apps1_travel,
    //            (double)in.apps2_raw, (double)out.apps2_travel,
    //            (double)out.accel_pedal_travel);

    uint32_t post_faults = vcu_can_get_inverter_post_faults();
    uint32_t run_faults  = vcu_can_get_inverter_run_faults();
    if (post_faults || run_faults) {
      log_printf(LOG_ERROR, "[INV] POST_FAULTS:0x%08lX RUN_FAULTS:0x%08lX\n",
                 post_faults, run_faults);
    }

    // 3 ms control loop (333 Hz)
    osDelay(pdMS_TO_TICKS(CONTROL_LOOP_PERIOD_MS));
  }
}

/* USER CODE END Application */
