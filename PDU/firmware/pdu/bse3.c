#include "bse3.h"

#include "cmsis_os.h"
#include "pdu_adc.h"
#include "pdu_can.h"

#define BSE3_DIVIDER_TOP_OHMS 10000.0f
#define BSE3_DIVIDER_BOTTOM_OHMS 27000.0f
#define BSE3_SENSOR_MIN_V 0.5f
#define BSE3_SENSOR_SPAN_V 4.0f
#define BSE3_MAX_PSI 3000.0f

static float clamp_f(float value, float min, float max) {
    if (value < min) {
        return min;
    }

    if (value > max) {
        return max;
    }

    return value;
}

static void bse3_task(void *arg);
static osThreadAttr_t bse3_task_attributes = {
    .name = "bse3Task",
    .priority = (osPriority_t)osPriorityNormal,
    .stack_size = 128 * 4,
};

void bse3_init(void) {
    osThreadNew(bse3_task, NULL, &bse3_task_attributes);
}

static void bse3_task(void *arg) {
    while (1) {
        pdu_can_set_bse3_pressure(bse3_pressure_psi());
        osDelay(3);
    }
}

float bse3_voltage(void) {
    return pdu_adc5_bse3_voltage();
}

float bse3_sensor_voltage(void) {
    return bse3_voltage() *
           ((BSE3_DIVIDER_TOP_OHMS + BSE3_DIVIDER_BOTTOM_OHMS) /
            BSE3_DIVIDER_BOTTOM_OHMS);
}

float bse3_pressure_psi(void) {
    float psi = ((bse3_sensor_voltage() - BSE3_SENSOR_MIN_V) /
                 BSE3_SENSOR_SPAN_V) *
                BSE3_MAX_PSI;

    return clamp_f(psi, 0.0f, BSE3_MAX_PSI);
}
