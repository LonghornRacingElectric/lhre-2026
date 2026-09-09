#include "led.h"

void led_init(TIM_TypeDef *tim1, TIM_HandleTypeDef *htim, int timchs) {
    ledtim = tim1;
    channels = timchs;

    HAL_TIM_PWM_Start(htim, TIM_CHANNEL_1);

    if (timchs <= 1) return;
    HAL_TIM_PWM_Start(htim, TIM_CHANNEL_2);

    if (timchs <= 2) return;
    HAL_TIM_PWM_Start(htim, TIM_CHANNEL_3);
}

static void led_setInt(uint8_t r, uint8_t g, uint8_t b) {
    if (channels <= 0) return;
    ledtim->CCR1 = b;
    if (channels <= 1) return;
    ledtim->CCR2 = r;
    if (channels <= 2) return;
    ledtim->CCR3 = g;
}

void led_set(float r, float g, float b) {
    led_setInt((uint8_t)(r * 255), (uint8_t)(g * 255), (uint8_t)(b * 255));
}

void led_off() { led_setInt(0, 0, 0); }

void led_rainbow(float deltaTime) {
    static uint8_t r = 255, g = 0, b = 0;
    static float timer = 0;

    uint32_t count;
    timer += deltaTime * 3.0f * 255.0f;
    if (timer > 3.0f) {
        // one cycle every 1.5 seconds
        count = (uint32_t)(timer / 3.0f);
        timer = 0;
        if (count > 10) count = 1;
    } else {
        return;
    }

    for (uint32_t i = 0; i < count; i++) {
        if ((r && g) || (!g && !b)) {
            r--;
            g++;
        } else if ((g && b) || (!b && !r)) {
            g--;
            b++;
        } else {
            b--;
            r++;
        }
    }

    led_setInt(r / 4, g / 2, b / 2);
}