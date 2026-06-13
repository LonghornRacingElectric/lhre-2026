#ifndef DUI_BUZZER_H
#define DUI_BUZZER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void dui_buzzer_init(void);
void dui_buzzer_tick(void);
uint32_t dui_buzzer_tick_count(void);
uint32_t dui_buzzer_active_tick_count(void);

#ifdef __cplusplus
}
#endif

#endif // DUI_BUZZER_H
