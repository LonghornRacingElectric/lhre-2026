#ifndef PDU_LINE_LOCK_H
#define PDU_LINE_LOCK_H

#ifdef __cplusplus
extern "C" {
#endif

void line_lock_init(void);
void line_lock_set_pwm(float percentage);

#ifdef __cplusplus
}
#endif

#endif  // PDU_LINE_LOCK_H
