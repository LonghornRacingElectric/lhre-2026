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

typedef struct {
    bool interfaceStarted;
    bool contactorStatusRegistered;
    bool contactorStateForced;
    uint32_t contactorStatusAgeMs;
    uint32_t messagesQueued;
    uint32_t droppedPackets;
    uint8_t contactorStatusData[4];
} hvc_can_tx_status_t;

void hvc_can_init(void);
void hvc_can_service_tx(void);
void hvc_can_periodic(bool amsError, bool imdError, int state,
                      float deltaTime);
void hvc_can_get_rx_status(hvc_can_rx_status_t *status);
void hvc_can_get_tx_status(hvc_can_tx_status_t *status);
float hvc_can_get_pack_soc(void);
bool hvc_can_is_charger_connected(void);
void hvc_can_set_charger_command(float maxChargeVoltage,
                                 float maxChargeCurrent, bool imdLed,
                                 bool bmsLed, bool enable);

#endif // HVC_CAN_H
