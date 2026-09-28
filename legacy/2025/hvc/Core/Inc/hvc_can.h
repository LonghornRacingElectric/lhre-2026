#ifndef HVC_CAN_H
#define HVC_CAN_H

#include <stdbool.h>

void hvc_can_init(void);
void hvc_can_periodic(bool amsError, bool imdError, int state,
                      float deltaTime);
bool hvc_can_is_charger_connected(void);
void hvc_can_set_charger_command(float maxChargeVoltage,
                                 float maxChargeCurrent, bool imdLed,
                                 bool bmsLed, bool enable);

#endif // HVC_CAN_H
