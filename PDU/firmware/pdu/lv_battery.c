#include "lv_battery.h"

#include "pdu_adc.h"

#define LV_BATTERY_DIVIDER_TOP_OHMS 10000.0f
#define LV_BATTERY_DIVIDER_BOTTOM_OHMS 1300.0f
#define LV_BATTERY_ALIVE_THRESHOLD_V 12.0f

float lv_battery_adc_voltage(void) {
    return pdu_adc5_sdwn1_voltage();
}

float lv_battery_voltage(void) {
    return lv_battery_adc_voltage() *
           ((LV_BATTERY_DIVIDER_TOP_OHMS + LV_BATTERY_DIVIDER_BOTTOM_OHMS) /
            LV_BATTERY_DIVIDER_BOTTOM_OHMS);
}

bool lv_battery_alive(void) {
    return lv_battery_voltage() >= LV_BATTERY_ALIVE_THRESHOLD_V;
}
