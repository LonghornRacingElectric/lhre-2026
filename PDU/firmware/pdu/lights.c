#include "lights.h"

#include <math.h>

#include "cmsis_os.h"
#include "main.h"
#include "pdu_can.h"
#include "tim.h"

// TSSI red LED PWM output
#define PWM_TSSI_R_INSTANCE htim2
#define PWM_TSSI_R_CHANNEL TIM_CHANNEL_1

// TSSI green LED PWM output
#define PWM_TSSI_G_INSTANCE htim2
#define PWM_TSSI_G_CHANNEL TIM_CHANNEL_3

// Brake light PWM output
#define PWM_BRAKE_LIGHT_INSTANCE htim5
#define PWM_BRAKE_LIGHT_CHANNEL TIM_CHANNEL_2

// TSSI behavior timing
#define TSSI_FAULT_BLINK_HALF_PERIOD_MS 200U
#define TSSI_NO_COMMS_GRACE_PERIOD_MS 4000U
#define LIGHTS_UPDATE_PERIOD_MS 100U

// Brake light voltage compensation
#define LIGHTS_NOMINAL_VOLTAGE 24.0f
#define LIGHTS_MINIMUM_VOLTAGE 12.0f
#define LIGHTS_VOLTAGE_EXPONENT 2.0f

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

static const osThreadAttr_t lightsTask_attributes = {
    .name = "lightsTask",
    .priority = (osPriority_t)osPriorityHigh,
    .stack_size = 128 * 8,
};

static void lights_update(void *argument);
static bool blink_is_on(uint32_t tick, uint32_t half_period_ms);
static bool hvc_fault_active(void);
static bool hvc_comms_timed_out(void);
static void update_tssi(uint32_t tick, uint32_t startup_tick);
static void set_tssi_normal(void);
static void set_tssi_fault(uint32_t tick);
static void set_tssi_no_comms(uint32_t tick, uint32_t startup_tick);

static bool tssi_fault_latched = false;

void lights_init(void) {
  // Start PWM outputs before the update task begins writing duty cycles.
  HAL_TIM_PWM_Start(&PWM_TSSI_R_INSTANCE, PWM_TSSI_R_CHANNEL);
  HAL_TIM_PWM_Start(&PWM_TSSI_G_INSTANCE, PWM_TSSI_G_CHANNEL);
  HAL_TIM_PWM_Start(&PWM_BRAKE_LIGHT_INSTANCE, PWM_BRAKE_LIGHT_CHANNEL);

  osThreadNew(lights_update, NULL, &lightsTask_attributes);
}

void set_red_light(bool on) {
  set_light(&PWM_TSSI_R_INSTANCE, PWM_TSSI_R_CHANNEL, on);
}

void set_green_light(bool on) {
  set_light(&PWM_TSSI_G_INSTANCE, PWM_TSSI_G_CHANNEL, on);
}

static bool blink_is_on(uint32_t tick, uint32_t half_period_ms) {
  return ((tick / half_period_ms) % 2U) == 0U;
}

static bool hvc_fault_active(void) {
  return hvc_imd_fault() || hvc_bms_fault();
}

static bool hvc_comms_timed_out(void) {
  return hvc_imd_timeout() || hvc_bms_timeout();
}

static void update_tssi(uint32_t tick, uint32_t startup_tick) {
  // IMD/BMS faults require a manual reset, so keep indicating them once seen.
  if (hvc_fault_active()) {
    tssi_fault_latched = true;
  }

  if (tssi_fault_latched) {
    set_tssi_fault(tick);
  } else if (hvc_comms_timed_out()) {
    set_tssi_no_comms(tick, startup_tick);
  } else {
    set_tssi_normal();
  }
}

static void set_tssi_normal(void) {
  set_green_light(true);
  set_red_light(false);
}

static void set_tssi_fault(uint32_t tick) {
  set_green_light(false);
  set_red_light(blink_is_on(tick, TSSI_FAULT_BLINK_HALF_PERIOD_MS));
}

static void set_tssi_no_comms(uint32_t tick, uint32_t startup_tick) {
  if (tick - startup_tick < TSSI_NO_COMMS_GRACE_PERIOD_MS) {
    set_tssi_normal();
    return;
  }

  set_red_light(false);
  set_green_light(false);
}

static void lights_update(void *argument) {
  (void)argument;

  const uint32_t startup_tick = osKernelGetTickCount();

  while (1) {
    update_tssi(osKernelGetTickCount(), startup_tick);

    setPWM(&PWM_BRAKE_LIGHT_INSTANCE, PWM_BRAKE_LIGHT_CHANNEL,
           brake_light_pct());

    osDelay(LIGHTS_UPDATE_PERIOD_MS);
  }
}

/**
 * @brief Adjusts a PWM percentage based on the current voltage to maintain
 * perceived brightness relative to a nominal voltage
 * * Assumes brightness is proportional to the square of the voltage (Power ~
 * V^2). The relationship might need tuning based on specific LED
 * characteristics.
 *
 * @param nominalPctAt24V The desired PWM percentage (0.0f to 1.0f) if the
 * voltage were exactly 24V.
 * @param curVoltage The actual measured voltage supplying the lights.
 * @return float The adjusted PWM percentage (0.0f to 1.0f) to apply at
 * curVoltage.
 */
float normalizeLightWithVoltage(float nominalPctAt24V, float curVoltage) {
  if (nominalPctAt24V <= 0.0f) {
    return 0.0f;
  }

  if (curVoltage < LIGHTS_MINIMUM_VOLTAGE) {
    return 0.0f;
  }

  const float voltageRatio = LIGHTS_NOMINAL_VOLTAGE / curVoltage;
  const float scalingFactor = powf(voltageRatio, LIGHTS_VOLTAGE_EXPONENT);
  const float adjustedPct = nominalPctAt24V * scalingFactor;

  return MAX(MIN(adjustedPct, 1.0f), 0.0f);
}

void set_light(TIM_HandleTypeDef *htim, uint32_t channel, bool on) {
  uint32_t period = __HAL_TIM_GET_AUTORELOAD(htim);
  uint32_t ccr_value = on ? period : 0U;
  __HAL_TIM_SET_COMPARE(htim, channel, ccr_value);
}

void setPWM(TIM_HandleTypeDef *htim, uint32_t channel, float percentage) {
  // Get timer period and calculate CCR based on duty cycle percentage
  uint32_t period = __HAL_TIM_GET_AUTORELOAD(htim);

  // TODO: implement voltage sense so we can use the real measured voltage
  uint32_t ccr_value =
      (uint32_t)(normalizeLightWithVoltage(percentage, 24.0f) * period);
  __HAL_TIM_SET_COMPARE(htim, channel, ccr_value);
}
