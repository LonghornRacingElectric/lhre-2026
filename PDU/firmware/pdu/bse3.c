#include "bse3.h"

#include "pdu_adc.h"

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
