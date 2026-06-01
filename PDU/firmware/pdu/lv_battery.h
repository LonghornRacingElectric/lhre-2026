#ifndef PDU_LV_BATTERY_H
#define PDU_LV_BATTERY_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

float lv_battery_adc_voltage(void);
float lv_battery_voltage(void);
bool lv_battery_alive(void);

#ifdef __cplusplus
}
#endif

#endif  // PDU_LV_BATTERY_H
