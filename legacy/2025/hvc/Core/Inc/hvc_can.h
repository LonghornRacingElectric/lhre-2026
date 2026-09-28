#ifndef HVC_CAN_H
#define HVC_CAN_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool vcuStateValid;
    uint32_t vcuStateAgeMs;
    uint8_t prndlState;
    bool stompFault;
    bool readyToDriveBuzzer;
    float stateOfChargeEstimate;
    bool lineLockEnabled;
    uint8_t eventMode;
} hvc_can_rx_status_t;

void hvc_can_init(void);
void hvc_can_periodic(bool amsError, bool imdError, int state,
                      float deltaTime);
void hvc_can_get_rx_status(hvc_can_rx_status_t *status);
bool hvc_can_is_charger_connected(void);
void hvc_can_set_charger_command(float maxChargeVoltage,
                                 float maxChargeCurrent, bool imdLed,
                                 bool bmsLed, bool enable);

#endif // HVC_CAN_H
