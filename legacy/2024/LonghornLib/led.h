#ifndef LONGHORN_LIBRARY_2024_LED_H
#define LONGHORN_LIBRARY_2024_LED_H

void led_init();

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

#endif //LONGHORN_LIBRARY_2024_LED_H
