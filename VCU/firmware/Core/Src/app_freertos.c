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

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define ADC_MAX_VAL ((1u << 12) - 1u)
#define ADC_APPS_SCALE_V 3.3f
#define ADC_BSE_SCALE_V 3.2837f
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

static vcu_parameters_t s_params = {
    .apps =
        {
            .apps1_min_adc_v = 1.750f,
            .apps1_max_adc_v = 1.520f,

            .apps2_min_adc_v = 0.190f,
            .apps2_max_adc_v = -0.020f,

            .implaus_debounce_time_ms = 100u,
            .max_allowable_diff = 0.15f,
            // .min_travel_threshold = 0.10f,
            // .max_travel_restore_threshold = 0.05f,

            .min_travel_deadzone = 0.09f,
            .max_travel_deadzone = 0.88f,
            .pedal_ema_alpha = 0.35f,
        },
    .bse =
        {
            .bse_off_psi = 30.0f,
            .bse_on_psi = 50.0f,
            .bse1_adc_at_min_psi_v =
                ((397.0f * ADC_BSE_SCALE_V) / ADC_MAX_VAL),
            .bse1_adc_at_max_psi_v =
                ((2267.0f * ADC_BSE_SCALE_V) / ADC_MAX_VAL),
            .bse2_adc_at_min_psi_v =
                ((397.0f * ADC_BSE_SCALE_V) /
                 ADC_MAX_VAL), 
            .bse2_adc_at_max_psi_v =
                ((2017.0f * ADC_BSE_SCALE_V) / ADC_MAX_VAL),
            .bse_max_psi = 3000.0f,
            .max_pedal_while_braking = 0.25f,
            .max_pedal_restore_threshold = 0.05f,
            .min_psi_deadzone = 0.4f,
            .max_psi_deadzone = 1.0f,
            .bse_ema_alpha = 0.10f,
            .brake_light_min_pct = 0.0f,
            .brake_light_max_pct = 0.30f,
        },
    .torque_map =
        {
            .power_limit_torque = {
              /* rpm=    0 */ 210.0f,
              /* rpm=  600 */ 210.0f,
              /* rpm= 1200 */ 210.0f,
              /* rpm= 1800 */ 210.0f,
              /* rpm= 2400 */ 210.0f,
              /* rpm= 3000 */ 192.0f,
              /* rpm= 3600 */ 166.0f,
              /* rpm= 4200 */ 145.0f,
              /* rpm= 4800 */ 129.0f,
              /* rpm= 5400 */ 115.0f,
              /* rpm= 6000 */ 103.0f
            },
            .pedal_curve_exponent = 2.0f,
            .low_cell_derate_start_v = 3.2f,
            .low_cell_cutoff_v = 3.0f,
        },
    .power_limit =
      {
          .power_limit_w = 70000.0f,
          .power_limit_trim_kp = 0.006f,
          .power_limit_trim_ki = 0.6f,
          .power_limit_trim_integral_max = 20000.0f,
      },
    .buzzer_duration_ms = 1200u,
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
  osDelay(pdMS_TO_TICKS(200));

  static uint32_t last_tick = 0;
  last_tick = osKernelGetTickCount();

  vcu_inputs_t in = {0};
  vcu_outputs_t out = {0};

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
    in.apps1_raw = ((float)adc3_dma_buf[0] * ADC_APPS_SCALE_V) / ADC_MAX_VAL;
    in.apps2_raw = ((float)adc3_dma_buf[1] * ADC_APPS_SCALE_V) / ADC_MAX_VAL;
    in.bse1_raw = ((float)adc2_dma_buf[0] * ADC_BSE_SCALE_V) / ADC_MAX_VAL;
    in.bse2_raw = ((float)adc2_dma_buf[1] * ADC_BSE_SCALE_V) / ADC_MAX_VAL;

    in.drive_switch = is_drive_switch_pressed();

    in.contactors_closed = hvc_tractive_ready();
    in.motor_speed_rpm = fabsf(vcu_can_get_motor_speed_rpm());
    in.min_cell_voltage_v = vcu_can_get_min_cell_voltage_v();
    if (in.min_cell_voltage_v <= 0.0f) {
      in.min_cell_voltage_v = s_params.torque_map.low_cell_derate_start_v;
    }
    in.battery_voltage_v = vcu_can_get_inverter_voltages().dc_bus;
    in.battery_current_a = vcu_can_get_inverter_currents().dc_bus;

    // Run control model
    vcu_model_step(&ctx, &in, &out, dt_ms);

    vcu_can_set_model_inputs(&in);
    vcu_can_set_model_outputs(&out);
    vcu_can_set_steering_angle_deg(steering_angle_deg);

    float delta_resolver_angle_deg = vcu_can_get_delta_resolver_angle_deg();
    float motor_angle_deg = vcu_can_get_motor_angle_deg();
    float torque_derate_pct = 1.0f;
    if (in.min_cell_voltage_v <= s_params.torque_map.low_cell_cutoff_v) {
      torque_derate_pct = 0.0f;
    } else if (in.min_cell_voltage_v <
                s_params.torque_map.low_cell_derate_start_v) {
      torque_derate_pct =
          (in.min_cell_voltage_v - s_params.torque_map.low_cell_cutoff_v) /
          (s_params.torque_map.low_cell_derate_start_v -
            s_params.torque_map.low_cell_cutoff_v);
    // log_printf(LOG_INFO,
    //          "TICK:%lu | RPM:%.0f DRA:%.1f ANG:%.1f PED:%.3f TQ:%.1f | "
    //          "MIN:%.4f DRT:%.2f | STR_RAW:%lu AV:%.3f SV:%.3f SPCT:%.3f "
    //          "STR_DEG:%.1f | "
    //          "PRNDL:%u INV:%u | "
    //          "DRV_IN:%u TR:%u | APPS_IMPL:%u BRAKE:%u ANYFLT:%u\n",
    //          (unsigned long)current_tick, (double)in.motor_speed_rpm,
    //          (double)delta_resolver_angle_deg, (double)motor_angle_deg,
    //          (double)out.accel_pedal_travel, (double)out.torque_cmd,
    //          (double)in.min_cell_voltage_v, (double)torque_derate_pct,
    //          (unsigned long)adc1_val, (double)steering_adc_voltage_v,
    //          (double)steering_sensor_voltage_v,
    //          (double)steering_angle_pct, (double)steering_angle_deg,
    //          (unsigned)out.prndl_state, (unsigned)out.inverter_enable,
    //          (unsigned)in.drive_switch, (unsigned)in.contactors_closed,
    //          (unsigned)out.faults.apps_any_fault,
    //          (unsigned)out.brake_pressed, (unsigned)out.faults.any_fault);
    
      log_printf(LOG_INFO,
             "\nAPPS1_RAW:%.3f APPS1_PCT:%.3f\nAPPS2_RAW:%.3f APPS2_PCT:%.3f\nAPPS: %.3f\n\n",
             (double)in.apps1_raw, (double)out.apps1_travel,
             (double)in.apps2_raw, (double)out.apps2_travel,
             (double)out.accel_pedal_travel);
    }

    // 3 ms control loop (333 Hz)
    osDelay(pdMS_TO_TICKS(CONTROL_LOOP_PERIOD_MS));
  }
}

/* USER CODE END Application */
