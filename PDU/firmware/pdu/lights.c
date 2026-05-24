#include "lights.h"

#include <math.h>

#include "cmsis_os.h"
#include "main.h"
#include "pdu_can.h"
#include "tim.h"

/**
 * TSSI Light Declarations
 */

// RED LED
#define PWM_TSSI_R_INSTANCE htim2
#define PWM_TSSI_R_CHANNEL TIM_CHANNEL_1

// GREEN LED
#define PWM_TSSI_G_INSTANCE htim2
#define PWM_TSSI_G_CHANNEL TIM_CHANNEL_3

#define PWM_BRAKE_LIGHT_INSTANCE htim5
#define PWM_BRAKE_LIGHT_CHANNEL TIM_CHANNEL_2

/**
 * Helper Defines
 */

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

/**
 * Light Task
 */

osThreadAttr_t lightsTask_attributes = {.name = "lightsTask",
                                        .priority =
                                            (osPriority_t)osPriorityHigh,
                                        .stack_size = 128 * 8};

void lights_update(void *argument);

void lights_init(void) {
  // Initialize PWM on LEDs
  HAL_TIM_PWM_Start(&PWM_TSSI_R_INSTANCE, PWM_TSSI_R_CHANNEL);
  // Start Task for updating LEDs based on CAN messages
  HAL_TIM_PWM_Start(&PWM_TSSI_G_INSTANCE, PWM_TSSI_G_CHANNEL);
  HAL_TIM_PWM_Start(&PWM_BRAKE_LIGHT_INSTANCE, PWM_BRAKE_LIGHT_CHANNEL);

  // Start LED Task
  osThreadNew(lights_update, NULL, &lightsTask_attributes);
}

void set_red_light(bool on) {
  if (on) {
    setPWM(&PWM_TSSI_R_INSTANCE, PWM_TSSI_R_CHANNEL, 0.10f);
  } else {
    setPWM(&PWM_TSSI_R_INSTANCE, PWM_TSSI_R_CHANNEL, 0.0f);
  }
}

void set_green_light(bool on) {
  if (on) {
    setPWM(&PWM_TSSI_G_INSTANCE, PWM_TSSI_G_CHANNEL, 0.10f);
  } else {
    setPWM(&PWM_TSSI_G_INSTANCE, PWM_TSSI_G_CHANNEL, 0.0f);
  }
}

void toggleRed() {
  static bool red_on = false;
  if (red_on) {
    // Turn it off
    set_red_light(false);
  } else {
    // Turn it on
    set_red_light(true);
  }
  red_on = !red_on;
}

void toggleGreen() {
  static bool green_on = false;
  if (green_on) {
    // Turn it off
    set_green_light(false);
  } else {
    // Turn it on
    set_green_light(true);
  }
  green_on = !green_on;
}

void lights_update(void *argument) {
  // Update LED states based on CAN messages
  static uint32_t previous_tick = 0;
  while (1) {
    if (hvc_imd_fault() || hvc_bms_fault()) {
      // Turn on Red LED and Disable Green LED
      set_green_light(false);
      if (osKernelGetTickCount() - previous_tick >= 300) {
        toggleRed();
        previous_tick = osKernelGetTickCount();
      }
    } else if (hvc_imd_timeout() || hvc_bms_timeout()) {
      // if anything times out, we just flash green
      set_red_light(false);
      if (osKernelGetTickCount() - previous_tick >= 300) {
        toggleGreen();
        previous_tick = osKernelGetTickCount();
      }
    } else {
      // Normal state
      set_green_light(true);
      set_red_light(false);
    }

    // Brake Light
    setPWM(&PWM_BRAKE_LIGHT_INSTANCE, PWM_BRAKE_LIGHT_CHANNEL,
           brake_light_pct());

    osDelay(100);
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
  const float nominalVoltage = 24.0f;
  const float voltageExponent = 2.0f; // Need to tune

  // 0, so doesn't matter, output 0
  if (nominalPctAt24V <= 0.0f) {
    return 0.0f;
  }

  if (curVoltage < 12.0f) {
    // LV is dead, don't PWM
    return 0.0f;
  }

  // Scaling the voltage
  float voltageRatio = nominalVoltage / curVoltage;
  float scalingFactor = powf(voltageRatio, voltageExponent);

  float adjustedPct = nominalPctAt24V * scalingFactor;

  // clamping output
  adjustedPct = MIN(adjustedPct, 1.0f);
  adjustedPct = MAX(adjustedPct, 0.0f);

  return adjustedPct;
}

void setPWM(TIM_HandleTypeDef *htim, uint32_t channel, float percentage) {
  // Get timer period and calculate CCR based on duty cycle percentage
  uint32_t period = __HAL_TIM_GET_AUTORELOAD(htim);

  // TODO: implement voltage sense so we can use the real measured voltage
  uint32_t ccr_value =
      (uint32_t)(normalizeLightWithVoltage(percentage, 24.0f) * period);
  __HAL_TIM_SET_COMPARE(htim, channel, ccr_value);
}
