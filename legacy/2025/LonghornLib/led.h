#ifndef LONGHORN_LIBRARY_2025_LED_H
#define LONGHORN_LIBRARY_2025_LED_H

#include "tim.h"

static TIM_TypeDef *ledtim;
static int channels;

void led_init(TIM_TypeDef *tim, TIM_HandleTypeDef *htim, int channels);

/**
 * Set debug LED to given RGB value (0-1 scale).
 * @param r red
 * @param g green
 * @param b blue
 */
void led_set(float r, float g, float b);

/**
 * Turn off debug LED.
 */
void led_off();

/**
 * Rainbow LED!
 * @param deltaTime how much time in seconds has passed since last function call
 */
void led_rainbow(float deltaTime);

#endif  // LONGHORN_LIBRARY_2025_LED_H
