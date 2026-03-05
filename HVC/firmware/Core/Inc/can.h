#ifndef __CAN_H__
#define __CAN_H__

#include <stdbool.h>

void hvc_can_init(void);

void hvc_set_contactor_status(int state, bool pos, bool neg);

#endif
