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

#define VCU_TORQUE_MAP_MAX_TORQUE_NM 220.0f
#define VCU_TORQUE_MAP_PEDAL_EXPONENTIAL_FACTOR 2.0f
#define VCU_TORQUE_MAP_POWER_LIMIT_W 80000.0f
#define VCU_TORQUE_MAP_CURRENT_LIMIT_A 200.0f
#define VCU_TORQUE_MAP_HARD_CURRENT_CUT_A 240.0f
#define VCU_TORQUE_MAP_HARD_POWER_CUT_W 85000.0f
#define VCU_TORQUE_MAP_OCV_CELL_COUNT 130.0f
#define VCU_TORQUE_MAP_OCV_LPF_TIME_CONSTANT_S 1.0f
#define VCU_TORQUE_MAP_CURRENT_LPF_TIME_CONSTANT_S 0.2f
#define VCU_TORQUE_MAP_MEASURED_POWER_LPF_TIME_CONSTANT_S 0.010f
#define VCU_TORQUE_MAP_POWER_LIMIT_MIN_RPM 100.0f
#define VCU_TORQUE_MAP_POWER_LIMIT_TRIM_LIMIT_NM 20.0f
#define VCU_TORQUE_MAP_POWER_LIMIT_KP 0.002f
#define VCU_TORQUE_MAP_POWER_LIMIT_KI 0.0f
#define VCU_TORQUE_MAP_POWER_LIMIT_KD 0.0f
#define VCU_TORQUE_MAP_LAUNCH_MODE_DISABLE false
#define VCU_TORQUE_MAP_LAUNCH_ENTER_RPM 10.0f
#define VCU_TORQUE_MAP_LAUNCH_EXIT_RPM 75.0f
#define VCU_TORQUE_MAP_LAUNCH_PEDAL_MIN 0.05f
#define VCU_TORQUE_MAP_LAUNCH_PEDAL_MAX 0.80f
#define VCU_TORQUE_MAP_LAUNCH_BRAKE_MIN_PSI 50.0f
#define VCU_TORQUE_MAP_LAUNCH_PRELOAD_TORQUE_NM 5.0f
#define VCU_TORQUE_MAP_LAUNCH_PRELOAD_RAMP_RATE_NM_PER_S 100.0f
#define VCU_TORQUE_MAP_LAUNCH_RELEASE_RAMP_RATE_NM_PER_S 350.0f
#define VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_0 0.0f
#define VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_1 550.0f
#define VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_2 1100.0f
#define VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_3 1650.0f
#define VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_4 2200.0f
#define VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_5 2750.0f
#define VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_6 3300.0f
#define VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_7 3850.0f
#define VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_8 4400.0f
#define VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_9 4950.0f
#define VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_10 5500.0f
#define VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_0 0.86f
#define VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_1 0.89f
#define VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_2 0.92f
#define VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_3 0.94f
#define VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_4 0.95f
#define VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_5 0.955f
#define VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_6 0.955f
#define VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_7 0.95f
#define VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_8 0.945f
#define VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_9 0.93f
#define VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_10 0.90f

#define VCU_TC_WHEEL_RADIUS_M 0.2f
#define VCU_TC_FINAL_DRIVE_RATIO 3.307f
#define VCU_TC_LONGITUDINAL_ADJUST 0.0f
#define VCU_TC_LATERAL_ADJUST 0.0f
#define VCU_TC_BASE_TARGET_SLIP 0.08f
#define VCU_TC_MIN_TARGET_SLIP 0.03f
#define VCU_TC_MAX_TARGET_SLIP 0.16f
#define VCU_TC_SLIP_HYSTERESIS 0.01f
#define VCU_TC_LATERAL_ACCEL_LIMIT_MPS2 11.0f
#define VCU_TC_AERO_LATERAL_ACCEL_GAIN_PER_MPS2 0.0025f
#define VCU_TC_LATERAL_SLIP_REDUCTION_GAIN 1.0f
#define VCU_TC_MIN_VEHICLE_SPEED_MPS 3.0f
#define VCU_TC_MIN_TORQUE_NM 5.0f
#define VCU_TC_MAX_WHEEL_SPEED_MPS 90.0f
#define VCU_TC_MAX_REFERENCE_ACCEL_MPS2 35.0f
#define VCU_TC_FRONT_DISAGREEMENT_MPS 3.0f
#define VCU_TC_REAR_DISAGREEMENT_MPS 8.0f
#define VCU_TC_MOTOR_REAR_DISAGREEMENT_MPS 8.0f
#define VCU_TC_SPEED_LPF_TIME_CONSTANT_S 0.035f
#define VCU_TC_SLIP_LPF_TIME_CONSTANT_S 0.020f
#define VCU_TC_FEEDBACK_LPF_TIME_CONSTANT_S 0.025f
#define VCU_TC_REFERENCE_ACCEL_BLEND 0.20f
#define VCU_TC_KP_NM_PER_SLIP 900.0f
#define VCU_TC_KI_NM_PER_SLIP_S 80.0f
#define VCU_TC_KD_NM_PER_SLIP_RATE 25.0f
#define VCU_TC_DRIVEN_ACCEL_GAIN_NM_PER_MPS2 2.0f
#define VCU_TC_INTEGRAL_LIMIT_NM 40.0f
#define VCU_TC_MAX_TORQUE_REDUCTION_NM 220.0f
#define VCU_TC_CUT_SLEW_NM_PER_S 2500.0f
#define VCU_TC_RECOVERY_SLEW_NM_PER_S 350.0f

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

static const vcu_parameters_t s_params = {
    .apps =
        {
            .apps1_min_adc_v =
                ((1560.0f * ADC_APPS_SCALE_V) / ADC_MAX_VAL),
            .apps1_max_adc_v =
                ((1255.0f * ADC_APPS_SCALE_V) / ADC_MAX_VAL),

            .apps2_min_adc_v =
                ((1560.0f * ADC_APPS_SCALE_V) / ADC_MAX_VAL),
            .apps2_max_adc_v =
                ((1240.0f * ADC_APPS_SCALE_V) / ADC_MAX_VAL),

            .implaus_debounce_time_ms = 100u,
            .max_allowable_diff = 0.12f,
            // .min_travel_threshold = 0.10f,
            // .max_travel_restore_threshold = 0.05f,

            .min_travel_deadzone = 0.14f,
            .max_travel_deadzone = 0.92f,
            .pedal_ema_alpha = 0.35f,
        },
    .torque_map =
        {
            .max_torque_nm = VCU_TORQUE_MAP_MAX_TORQUE_NM,
            .pedal_exponential_factor =
                VCU_TORQUE_MAP_PEDAL_EXPONENTIAL_FACTOR,
            .power_limit_w = VCU_TORQUE_MAP_POWER_LIMIT_W,
            .current_limit_a = VCU_TORQUE_MAP_CURRENT_LIMIT_A,
            .hard_current_cut_a = VCU_TORQUE_MAP_HARD_CURRENT_CUT_A,
            .hard_power_cut_w = VCU_TORQUE_MAP_HARD_POWER_CUT_W,
            .ocv_cell_count = VCU_TORQUE_MAP_OCV_CELL_COUNT,
            .ocv_lpf_time_constant_s =
                VCU_TORQUE_MAP_OCV_LPF_TIME_CONSTANT_S,
            .current_lpf_time_constant_s =
                VCU_TORQUE_MAP_CURRENT_LPF_TIME_CONSTANT_S,
            .measured_power_lpf_time_constant_s =
                VCU_TORQUE_MAP_MEASURED_POWER_LPF_TIME_CONSTANT_S,
            .power_limit_min_rpm = VCU_TORQUE_MAP_POWER_LIMIT_MIN_RPM,
            .power_limit_trim_limit_nm =
                VCU_TORQUE_MAP_POWER_LIMIT_TRIM_LIMIT_NM,
            .power_limit_kp = VCU_TORQUE_MAP_POWER_LIMIT_KP,
            .power_limit_ki = VCU_TORQUE_MAP_POWER_LIMIT_KI,
            .power_limit_kd = VCU_TORQUE_MAP_POWER_LIMIT_KD,
            .launch_mode_disable = VCU_TORQUE_MAP_LAUNCH_MODE_DISABLE,
            .launch_enter_rpm = VCU_TORQUE_MAP_LAUNCH_ENTER_RPM,
            .launch_exit_rpm = VCU_TORQUE_MAP_LAUNCH_EXIT_RPM,
            .launch_pedal_min = VCU_TORQUE_MAP_LAUNCH_PEDAL_MIN,
            .launch_pedal_max = VCU_TORQUE_MAP_LAUNCH_PEDAL_MAX,
            .launch_brake_min_psi = VCU_TORQUE_MAP_LAUNCH_BRAKE_MIN_PSI,
            .launch_preload_torque_nm =
                VCU_TORQUE_MAP_LAUNCH_PRELOAD_TORQUE_NM,
            .launch_preload_ramp_rate_nm_per_s =
                VCU_TORQUE_MAP_LAUNCH_PRELOAD_RAMP_RATE_NM_PER_S,
            .launch_release_ramp_rate_nm_per_s =
                VCU_TORQUE_MAP_LAUNCH_RELEASE_RAMP_RATE_NM_PER_S,
            .power_limit_motor_efficiency_rpm =
                {
                    VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_0,
                    VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_1,
                    VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_2,
                    VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_3,
                    VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_4,
                    VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_5,
                    VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_6,
                    VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_7,
                    VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_8,
                    VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_9,
                    VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_10,
                },
            .power_limit_motor_efficiency =
                {
                    VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_0,
                    VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_1,
                    VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_2,
                    VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_3,
                    VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_4,
                    VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_5,
                    VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_6,
                    VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_7,
                    VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_8,
                    VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_9,
                    VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_10,
                },
        },

    .traction_control =
        {
            .tc_disable = true,
            .tc_use_accel = false,
            .tc_aero_lateral_limit_enable = false,
            .tc_wheel_radius_m = VCU_TC_WHEEL_RADIUS_M,
            .tc_final_drive_ratio = VCU_TC_FINAL_DRIVE_RATIO,
            .tc_longitudinal_adjust = VCU_TC_LONGITUDINAL_ADJUST,
            .tc_lateral_adjust = VCU_TC_LATERAL_ADJUST,
            .tc_base_target_slip = VCU_TC_BASE_TARGET_SLIP,
            .tc_min_target_slip = VCU_TC_MIN_TARGET_SLIP,
            .tc_max_target_slip = VCU_TC_MAX_TARGET_SLIP,
            .tc_slip_hysteresis = VCU_TC_SLIP_HYSTERESIS,
            .tc_lateral_accel_limit_mps2 = VCU_TC_LATERAL_ACCEL_LIMIT_MPS2,
            .tc_aero_lateral_accel_gain_per_mps2 =
                VCU_TC_AERO_LATERAL_ACCEL_GAIN_PER_MPS2,
            .tc_lateral_slip_reduction_gain =
                VCU_TC_LATERAL_SLIP_REDUCTION_GAIN,
            .tc_min_vehicle_speed_mps = VCU_TC_MIN_VEHICLE_SPEED_MPS,
            .tc_min_torque_nm = VCU_TC_MIN_TORQUE_NM,
            .tc_max_wheel_speed_mps = VCU_TC_MAX_WHEEL_SPEED_MPS,
            .tc_max_reference_accel_mps2 = VCU_TC_MAX_REFERENCE_ACCEL_MPS2,
            .tc_front_disagreement_mps = VCU_TC_FRONT_DISAGREEMENT_MPS,
            .tc_rear_disagreement_mps = VCU_TC_REAR_DISAGREEMENT_MPS,
            .tc_motor_rear_disagreement_mps =
                VCU_TC_MOTOR_REAR_DISAGREEMENT_MPS,
            .tc_speed_lpf_time_constant_s = VCU_TC_SPEED_LPF_TIME_CONSTANT_S,
            .tc_slip_lpf_time_constant_s = VCU_TC_SLIP_LPF_TIME_CONSTANT_S,
            .tc_feedback_lpf_time_constant_s =
                VCU_TC_FEEDBACK_LPF_TIME_CONSTANT_S,
            .tc_reference_accel_blend = VCU_TC_REFERENCE_ACCEL_BLEND,
            .tc_kp_nm_per_slip = VCU_TC_KP_NM_PER_SLIP,
            .tc_ki_nm_per_slip_s = VCU_TC_KI_NM_PER_SLIP_S,
            .tc_kd_nm_per_slip_rate = VCU_TC_KD_NM_PER_SLIP_RATE,
            .tc_driven_accel_gain_nm_per_mps2 =
                VCU_TC_DRIVEN_ACCEL_GAIN_NM_PER_MPS2,
            .tc_integral_limit_nm = VCU_TC_INTEGRAL_LIMIT_NM,
            .tc_max_torque_reduction_nm = VCU_TC_MAX_TORQUE_REDUCTION_NM,
            .tc_cut_slew_nm_per_s = VCU_TC_CUT_SLEW_NM_PER_S,
            .tc_recovery_slew_nm_per_s = VCU_TC_RECOVERY_SLEW_NM_PER_S,
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
            .max_pedal_while_braking = 0.30f,
            .max_pedal_restore_threshold = 0.05f,
            .min_psi_deadzone = 0.4f,
            .max_psi_deadzone = 1.0f,
            .bse_ema_alpha = 1.0f,
            .brake_light_min_pct = 0.0f,
            .brake_light_max_pct = 0.30f,
        },
    .buzzer_duration_ms = 1800u,
    .brake_enable_threshold = 0.1f,
};

_Static_assert(VCU_TORQUE_MAP_MAX_TORQUE_NM > 0.0f,
               "VCU torque_map.max_torque_nm must be positive");
_Static_assert(VCU_TORQUE_MAP_PEDAL_EXPONENTIAL_FACTOR >= 0.0f,
               "VCU torque_map.pedal_exponential_factor cannot be negative");
_Static_assert(VCU_TORQUE_MAP_POWER_LIMIT_W > 0.0f,
               "VCU torque_map.power_limit_w must be positive");
_Static_assert(VCU_TORQUE_MAP_CURRENT_LIMIT_A > 0.0f,
               "VCU torque_map.current_limit_a must be positive");
_Static_assert(VCU_TORQUE_MAP_HARD_CURRENT_CUT_A >=
                   VCU_TORQUE_MAP_CURRENT_LIMIT_A,
               "VCU torque_map.hard_current_cut_a must be >= current_limit_a");
_Static_assert(VCU_TORQUE_MAP_HARD_POWER_CUT_W >=
                   VCU_TORQUE_MAP_POWER_LIMIT_W,
               "VCU torque_map.hard_power_cut_w must be >= power_limit_w");
_Static_assert(VCU_TORQUE_MAP_OCV_CELL_COUNT > 0.0f,
               "VCU torque_map.ocv_cell_count must be positive");
_Static_assert(VCU_TORQUE_MAP_OCV_LPF_TIME_CONSTANT_S >= 0.0f,
               "VCU torque_map.ocv_lpf_time_constant_s cannot be negative");
_Static_assert(VCU_TORQUE_MAP_CURRENT_LPF_TIME_CONSTANT_S >= 0.0f,
               "VCU torque_map.current_lpf_time_constant_s cannot be negative");
_Static_assert(VCU_TORQUE_MAP_MEASURED_POWER_LPF_TIME_CONSTANT_S >= 0.0f,
               "VCU torque_map.measured_power_lpf_time_constant_s cannot be negative");
_Static_assert(VCU_TORQUE_MAP_POWER_LIMIT_MIN_RPM > 0.0f,
               "VCU torque_map.power_limit_min_rpm must be positive");
_Static_assert(VCU_TORQUE_MAP_POWER_LIMIT_TRIM_LIMIT_NM >= 0.0f,
               "VCU torque_map.power_limit_trim_limit_nm cannot be negative");
_Static_assert(VCU_TORQUE_MAP_POWER_LIMIT_KP >= 0.0f,
               "VCU torque_map.power_limit_kp cannot be negative");
_Static_assert(VCU_TORQUE_MAP_POWER_LIMIT_KI >= 0.0f,
               "VCU torque_map.power_limit_ki cannot be negative");
_Static_assert(VCU_TORQUE_MAP_POWER_LIMIT_KD >= 0.0f,
               "VCU torque_map.power_limit_kd cannot be negative");
_Static_assert(VCU_TORQUE_MAP_LAUNCH_ENTER_RPM >= 0.0f,
               "VCU torque_map.launch_enter_rpm cannot be negative");
_Static_assert(VCU_TORQUE_MAP_LAUNCH_EXIT_RPM >
                   VCU_TORQUE_MAP_LAUNCH_ENTER_RPM,
               "VCU torque_map.launch_exit_rpm must be > launch_enter_rpm");
_Static_assert(VCU_TORQUE_MAP_LAUNCH_PEDAL_MIN >= 0.0f &&
                   VCU_TORQUE_MAP_LAUNCH_PEDAL_MIN <= 1.0f,
               "VCU torque_map.launch_pedal_min must be in [0, 1]");
_Static_assert(VCU_TORQUE_MAP_LAUNCH_PEDAL_MAX >=
                       VCU_TORQUE_MAP_LAUNCH_PEDAL_MIN &&
                   VCU_TORQUE_MAP_LAUNCH_PEDAL_MAX <= 1.0f,
               "VCU torque_map.launch_pedal_max must be in [launch_pedal_min, 1]");
_Static_assert(VCU_TORQUE_MAP_LAUNCH_BRAKE_MIN_PSI >= 0.0f,
               "VCU torque_map.launch_brake_min_psi cannot be negative");
_Static_assert(VCU_TORQUE_MAP_LAUNCH_PRELOAD_TORQUE_NM >= 0.0f,
               "VCU torque_map.launch_preload_torque_nm cannot be negative");
_Static_assert(VCU_TORQUE_MAP_LAUNCH_PRELOAD_RAMP_RATE_NM_PER_S > 0.0f,
               "VCU torque_map.launch_preload_ramp_rate_nm_per_s must be positive");
_Static_assert(VCU_TORQUE_MAP_LAUNCH_RELEASE_RAMP_RATE_NM_PER_S > 0.0f,
               "VCU torque_map.launch_release_ramp_rate_nm_per_s must be positive");
_Static_assert(VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_0 >= 0.0f &&
                   VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_1 >
                       VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_0 &&
                   VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_2 >
                       VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_1 &&
                   VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_3 >
                       VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_2 &&
                   VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_4 >
                       VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_3 &&
                   VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_5 >
                       VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_4 &&
                   VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_6 >
                       VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_5 &&
                   VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_7 >
                       VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_6 &&
                   VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_8 >
                       VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_7 &&
                   VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_9 >
                       VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_8 &&
                   VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_10 >
                       VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_RPM_9,
               "VCU torque_map.power_limit_motor_efficiency_rpm entries must increase");
_Static_assert(VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_0 > 0.0f &&
                   VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_0 <= 1.0f &&
                   VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_1 > 0.0f &&
                   VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_1 <= 1.0f &&
                   VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_2 > 0.0f &&
                   VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_2 <= 1.0f &&
                   VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_3 > 0.0f &&
                   VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_3 <= 1.0f &&
                   VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_4 > 0.0f &&
                   VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_4 <= 1.0f &&
                   VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_5 > 0.0f &&
                   VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_5 <= 1.0f &&
                   VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_6 > 0.0f &&
                   VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_6 <= 1.0f &&
                   VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_7 > 0.0f &&
                   VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_7 <= 1.0f &&
                   VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_8 > 0.0f &&
                   VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_8 <= 1.0f &&
                   VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_9 > 0.0f &&
                   VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_9 <= 1.0f &&
                   VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_10 > 0.0f &&
                   VCU_TORQUE_MAP_POWER_LIMIT_MOTOR_EFFICIENCY_10 <= 1.0f,
               "VCU torque_map.power_limit_motor_efficiency entries must be in (0, 1]");
_Static_assert(VCU_TC_WHEEL_RADIUS_M > 0.0f,
               "VCU traction_control.tc_wheel_radius_m must be positive");
_Static_assert(VCU_TC_FINAL_DRIVE_RATIO > 0.0f,
               "VCU traction_control.tc_final_drive_ratio must be positive");
_Static_assert(VCU_TC_MIN_TARGET_SLIP > 0.0f,
               "VCU traction_control.tc_min_target_slip must be positive");
_Static_assert(VCU_TC_BASE_TARGET_SLIP >= VCU_TC_MIN_TARGET_SLIP,
               "VCU traction_control.tc_base_target_slip must be >= tc_min_target_slip");
_Static_assert(VCU_TC_MAX_TARGET_SLIP >= VCU_TC_BASE_TARGET_SLIP,
               "VCU traction_control.tc_max_target_slip must be >= tc_base_target_slip");
_Static_assert(VCU_TC_SLIP_HYSTERESIS >= 0.0f,
               "VCU traction_control.tc_slip_hysteresis cannot be negative");
_Static_assert(VCU_TC_LATERAL_ACCEL_LIMIT_MPS2 > 0.0f,
               "VCU traction_control.tc_lateral_accel_limit_mps2 must be positive");
_Static_assert(VCU_TC_AERO_LATERAL_ACCEL_GAIN_PER_MPS2 >= 0.0f,
               "VCU traction_control.tc_aero_lateral_accel_gain_per_mps2 cannot be negative");
_Static_assert(VCU_TC_LATERAL_SLIP_REDUCTION_GAIN >= 0.0f,
               "VCU traction_control.tc_lateral_slip_reduction_gain cannot be negative");
_Static_assert(VCU_TC_MIN_VEHICLE_SPEED_MPS > 0.0f,
               "VCU traction_control.tc_min_vehicle_speed_mps must be positive");
_Static_assert(VCU_TC_MIN_TORQUE_NM >= 0.0f,
               "VCU traction_control.tc_min_torque_nm cannot be negative");
_Static_assert(VCU_TC_MAX_WHEEL_SPEED_MPS > 0.0f,
               "VCU traction_control.tc_max_wheel_speed_mps must be positive");
_Static_assert(VCU_TC_MAX_REFERENCE_ACCEL_MPS2 > 0.0f,
               "VCU traction_control.tc_max_reference_accel_mps2 must be positive");
_Static_assert(VCU_TC_FRONT_DISAGREEMENT_MPS > 0.0f,
               "VCU traction_control.tc_front_disagreement_mps must be positive");
_Static_assert(VCU_TC_REAR_DISAGREEMENT_MPS > 0.0f,
               "VCU traction_control.tc_rear_disagreement_mps must be positive");
_Static_assert(VCU_TC_MOTOR_REAR_DISAGREEMENT_MPS > 0.0f,
               "VCU traction_control.tc_motor_rear_disagreement_mps must be positive");
_Static_assert(VCU_TC_SPEED_LPF_TIME_CONSTANT_S >= 0.0f,
               "VCU traction_control.tc_speed_lpf_time_constant_s cannot be negative");
_Static_assert(VCU_TC_SLIP_LPF_TIME_CONSTANT_S >= 0.0f,
               "VCU traction_control.tc_slip_lpf_time_constant_s cannot be negative");
_Static_assert(VCU_TC_FEEDBACK_LPF_TIME_CONSTANT_S >= 0.0f,
               "VCU traction_control.tc_feedback_lpf_time_constant_s cannot be negative");
_Static_assert(VCU_TC_REFERENCE_ACCEL_BLEND >= 0.0f &&
                   VCU_TC_REFERENCE_ACCEL_BLEND <= 1.0f,
               "VCU traction_control.tc_reference_accel_blend must be within [0, 1]");
_Static_assert(VCU_TC_KP_NM_PER_SLIP >= 0.0f,
               "VCU traction_control.tc_kp_nm_per_slip cannot be negative");
_Static_assert(VCU_TC_KI_NM_PER_SLIP_S >= 0.0f,
               "VCU traction_control.tc_ki_nm_per_slip_s cannot be negative");
_Static_assert(VCU_TC_KD_NM_PER_SLIP_RATE >= 0.0f,
               "VCU traction_control.tc_kd_nm_per_slip_rate cannot be negative");
_Static_assert(VCU_TC_DRIVEN_ACCEL_GAIN_NM_PER_MPS2 >= 0.0f,
               "VCU traction_control.tc_driven_accel_gain_nm_per_mps2 cannot be negative");
_Static_assert(VCU_TC_INTEGRAL_LIMIT_NM >= 0.0f,
               "VCU traction_control.tc_integral_limit_nm cannot be negative");
_Static_assert(VCU_TC_MAX_TORQUE_REDUCTION_NM >= 0.0f,
               "VCU traction_control.tc_max_torque_reduction_nm cannot be negative");
_Static_assert(VCU_TC_CUT_SLEW_NM_PER_S > 0.0f,
               "VCU traction_control.tc_cut_slew_nm_per_s must be positive");
_Static_assert(VCU_TC_RECOVERY_SLEW_NM_PER_S > 0.0f,
               "VCU traction_control.tc_recovery_slew_nm_per_s must be positive");

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
    // osDelay(1);

    // To-Do: Steering
    // HAL_ADC_Start(&hadc1);
    // if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
    //   adc1_val = HAL_ADC_GetValue(&hadc1);
    // }
    // HAL_ADC_Stop(&hadc1);

    // Read pedal sensors from DMA buffers
    in.apps1_raw = ((float)adc3_dma_buf[0] * ADC_APPS_SCALE_V) / ADC_MAX_VAL;
    in.apps2_raw = ((float)adc3_dma_buf[1] * ADC_APPS_SCALE_V) / ADC_MAX_VAL;
    in.bse1_raw = ((float)adc2_dma_buf[0] * ADC_BSE_SCALE_V) / ADC_MAX_VAL;
    in.bse2_raw = ((float)adc2_dma_buf[1] * ADC_BSE_SCALE_V) / ADC_MAX_VAL;

    in.drive_switch = is_drive_switch_pressed();

    in.contactors_closed = hvc_tractive_ready();
    vcu_can_set_powertrain_inputs(&in);

    // Run control model
    vcu_model_step(&ctx, &in, &out, dt_ms);

    vcu_can_set_model_inputs(&in);
    vcu_can_set_model_outputs(&out);

    // log_printf(LOG_WARNING,
    //            "PEDAL OUT, %0.2f, TORQUE OUT %0.2f, FAULT %d, APPS1 %0.2f, "
    //            "APPS2 %0.2f",
    //            out.accel_pedal_travel, out.torque_cmd,
    //            out.faults.apps_any_fault, out.apps1_travel,
    //            out.apps2_travel);

float front_bias_pct = 0.0f;
float total = out.bse1_psi + out.bse2_psi;

if (total > 1e-3f) {
    front_bias_pct = (out.bse1_psi / total) * 100.0f;
}

// log_printf(LOG_WARNING,
//     "BSE_RAW adc1=%u adc2=%u | PSI1=%.2f PSI2=%.2f AVG=%.2f | FRONT_BIAS=%.1f%% | BRAKE=%u",
//     (unsigned)adc2_dma_buf[0],
//     (unsigned)adc2_dma_buf[1],
//     out.bse1_psi,
//     out.bse2_psi,
//     out.bse_psi,
//     front_bias_pct,
//     (unsigned)out.brake_pressed
// );

//     log_printf(LOG_WARNING,
//     "BSE_RAW adc1=%u adc2=%u | V1=%.4f V2=%.4f | CAL1 min=%.4f max=%.4f | CAL2 min=%.4f max=%.4f | PSI1=%.2f PSI2=%.2f AVG=%.2f | BRAKE=%u",
//     (unsigned)adc2_dma_buf[0],
//     (unsigned)adc2_dma_buf[1],
//     in.bse1_raw,
//     in.bse2_raw,
//     s_params.bse.bse1_adc_at_min_psi_v,
//     s_params.bse.bse1_adc_at_max_psi_v,
//     s_params.bse.bse2_adc_at_min_psi_v,
//     s_params.bse.bse2_adc_at_max_psi_v,
//     out.bse1_psi,
//     out.bse2_psi,
//     out.bse_psi,
//     (unsigned)out.brake_pressed
// );
    
    // log_printf(LOG_WARNING,
    // "PEDAL_OUT %.2f, TORQUE_OUT %.2f, APPS1 %.2f, APPS2 %.2f, adc1: %u, adc2: %u",
    // out.accel_pedal_travel,
    // out.torque_cmd,
    // out.apps1_travel,
    // out.apps2_travel,
    // adc3_dma_buf[0],
    // adc3_dma_buf[1]);           

    log_printf(LOG_INFO,
               "TICK:%lu | PED:%.3f TQ:%.1f | PRNDL:%u INV:%u | "
               "DRV_IN:%u TR:%u | PWR:%.0f LIM:%.0f OCV:%.1f | "
               "LST:%u LRAW:%.1f LTQ:%.1f | "
               "APPS_IMPL:%u BRAKE:%u ANYFLT:%u PLFLT:%u\n",
               (unsigned long)current_tick, (double)out.accel_pedal_travel,
               (double)out.torque_cmd, (unsigned)out.prndl_state,
               (unsigned)out.inverter_enable, (unsigned)in.drive_switch,
               (unsigned)in.contactors_closed, (double)out.debug.measured_power_w,
               (double)out.debug.active_power_limit_w,
               (double)out.debug.ocv_estimate_v,
               (unsigned)out.debug.launch_state,
               (double)out.debug.launch_raw_torque_cmd_nm,
               (double)out.debug.launch_torque_cmd_nm,
               (unsigned)out.faults.apps_any_fault,
               (unsigned)out.brake_pressed, (unsigned)out.faults.any_fault,
               (unsigned)out.faults.power_limit_input_fault);

    // log_printf(LOG_WARNING,
    //        "R2D_RX:%u CONTACTORS:%u PRNDL:%u BRAKE:%u\n",
    //        (unsigned)in.drive_switch,
    //        (unsigned)in.contactors_closed,
    //        (unsigned)out.prndl_state,
    //        (unsigned)out.brake_pressed);

    

    // 3 ms control loop (333 Hz)
    osDelay(pdMS_TO_TICKS(3));
  }
}

/* USER CODE END Application */
