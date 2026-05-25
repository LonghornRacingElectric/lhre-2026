#ifndef PDU_LINE_LOCK_H
#define PDU_LINE_LOCK_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool leg_11;
    bool leg_12;
    bool leg_13;
    bool leg_14;
    bool leg_15;
} line_lock_shutdown_sense_t;

void line_lock_init(void);
void line_lock_set_pwm(float percentage);
float line_lock_bse3_voltage(void);
line_lock_shutdown_sense_t line_lock_shutdown_sense(void);
bool line_lock_shutdown_closed(void);

#ifdef __cplusplus
}
#endif

#endif  // PDU_LINE_LOCK_H
