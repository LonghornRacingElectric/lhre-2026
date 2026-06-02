#ifndef DUI_INDICATOR_LIGHTS_H
#define DUI_INDICATOR_LIGHTS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Starts the task that updates the BMS and IMD error lights from CAN.
 */
void dui_indicator_lights_init(void);

#ifdef __cplusplus
}
#endif

#endif // DUI_INDICATOR_LIGHTS_H
