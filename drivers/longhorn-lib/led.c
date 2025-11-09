#include "led.h"

#include <math.h>

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"

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
    float phase_advance = deltaTime * (RAINBOW_PHASE_RANGE / RAINBOW_CYCLE_TIME_S);
    phase += phase_advance;
    while (phase >= RAINBOW_PHASE_RANGE) {
        phase -= RAINBOW_PHASE_RANGE;
    }

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

void RainbowLED(void* argument) {
    const uint32_t periodTicks = pdMS_TO_TICKS(33);
    const float secondsPerTick = 1.0f / (float)configTICK_RATE_HZ;
    uint32_t lastRunTime = osKernelGetTickCount();
    uint32_t nextWakeTime = lastRunTime;

    for (;;) {
        // absolute time that the rainbow task should be woken up again
        nextWakeTime += periodTicks;
        osDelayUntil(nextWakeTime);

        uint32_t currentRunTime = osKernelGetTickCount();
        uint32_t elapsedTicks = currentRunTime - lastRunTime;
        float elapsedSeconds = (float)elapsedTicks * secondsPerTick;

        // rainbow with however much time actually elapsed
        led_rainbow(elapsedSeconds);
        lastRunTime = currentRunTime;
    }
}

void setup_channel(volatile uint32_t* ccr, unsigned int channel) {
    if (ccr) {
        // make sure we actually have a channel, and then start it
        // we start the LED with a mid-brightness white so that we know the
        // board is booted
        *ccr = 125;
        led.pwm_start(led.timer_handle, channel);
    }
}

void led_init(rainbow_led_t config) {
    led = config;
    // Setup all the channels
    setup_channel(led.ccr1, led.channel1);
    setup_channel(led.ccr2, led.channel2);
    setup_channel(led.ccr3, led.channel3);
}

const osThreadAttr_t led_attributes = {
    .name = "LED Rainbow",
    .priority = (osPriority_t)osPriorityNormal,
    .stack_size = configMINIMAL_STACK_SIZE * 2};

osThreadId_t led_start_thread() {
    osThreadNew(RainbowLED, NULL, &led_attributes);
}
