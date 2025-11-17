#include "longhorn/led_base.h"

#include "math.h"

static rainbow_led_t led = {};

static void led_setInt(uint8_t r, uint8_t g, uint8_t b) {
    *(led.ccr1) = r;
    *(led.ccr2) = g;
    *(led.ccr3) = b;
}

void led_set(float r, float g, float b) {
    led_setInt((uint8_t)(r * 255), (uint8_t)(g * 255), (uint8_t)(b * 255));
}

void led_off() { led_setInt(0, 0, 0); }

void led_rainbow(float deltaTime) {
    // keep track of LED rainbow phase so we can step forward into the rainbow
    static float phase = 0.0f;
    const float RAINBOW_PHASE_RANGE = 3.0f;
    float phase_advance =
        deltaTime * (RAINBOW_PHASE_RANGE / RAINBOW_CYCLE_TIME_S);
    phase = fmodf(phase + phase_advance, RAINBOW_PHASE_RANGE);

    float r = 0.0f, g = 0.0f, b = 0.0f;

    if (phase < 1.0f) {
        r = 1.0f - phase;
        g = phase;
    } else if (phase < 2.0f) {
        float p = phase - 1.0f;
        g = 1.0f - p;
        b = p;
    } else {
        float p = phase - 2.0f;
        b = 1.0f - p;
        r = p;
    }

    // Normalize brightness so we don't blind ourselves
    led_set(r * 0.5f, g * 0.5f, b * 0.5f);
}

static void setup_channel(volatile uint32_t* ccr, unsigned int channel) {
    if (ccr) {
        // make sure we actually have a channel, and then start it
        // we start the LED with a mid-brightness white so that we know the
        // board is booted
        *ccr = 125;
        led.pwm_start(led.timer_handle, channel);
    }
}

void led_init(const rainbow_led_t* config) {
    led = *config;
    // Setup all the channels
    setup_channel(led.ccr1, led.channel1);
    setup_channel(led.ccr2, led.channel2);
    setup_channel(led.ccr3, led.channel3);
}
