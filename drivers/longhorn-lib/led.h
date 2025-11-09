#ifndef LONGHORN_LIBRARY_2025_LED_H
#define LONGHORN_LIBRARY_2025_LED_H

#include <stdint.h>

#include "cmsis_os2.h"

/** Time in seconds we want the Rainbow LED to cycle back to starting color */
#define RAINBOW_CYCLE_TIME_S 5.0f

typedef void (*HAL_PWM_Start_Fn)(void* htim, uint32_t Channel);

/**
 * Rainbow LED Configuration Struct
 */
typedef struct rainbow_led {
    volatile uint32_t* ccr1; /** Timer Capture/Compare Register 1 (CCR1) */
    volatile uint32_t* ccr2; /** Timer Capture/Compare Register 2 (CCR2) */
    volatile uint32_t* ccr3; /** Timer Capture/Compare Register 3 (CCR3) */
    void* timer_handle;      /** Timer Pointer (e.g. &htim2) */
    /** HAL PWM Start Function Pointer (e.g. HAL_TIM_PWM_START) */
    HAL_PWM_Start_Fn pwm_start;

    /** Timer Channels (e.g. TIM_CHANNEL_1) */
    unsigned int channel1;
    unsigned int channel2;
    unsigned int channel3;
} rainbow_led_t;

/**
 * @brief Sets the LED to a specified color/brightness, where RGB values are
 * percentage brightness
 *
 * @param r percent brightness of the RED color
 * @param g percent brightness of the GREEN color
 * @param b percent brightness of the BLUE color
 */
void led_set(float r, float g, float b);

/**
 * @brief Initializes the LED with the rainbow_led_t configuration.
 *
 * @param config rainbow_led_t configuration for the LED to run.
 */
void led_init(rainbow_led_t config);

/**
 * @brief Starts a new thread that runs every 33ms for the LED to rainbow.
 *
 *
 * @return osThreadId_t ID of the newly created thread/task in FreeRTOS
 */
osThreadId_t led_start_thread();

#endif  // LONGHORN_LIBRARY_2025_LED_H