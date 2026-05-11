//
// Created by Ashwin Kuppahally on 3/24/25.
// Ported 2026 to the new HVC <-> Charger CAN protocol (IDs 0x200 / 0x201,
// see drivers/longhorn-lib/config/can_packets.csv).
//

#ifndef CHARGING_H
#define CHARGING_H
#include <stdbool.h>

void charging_init(void);
void charging_periodic(float dt);

// Front-panel BMS/IMD LED state, sourced from the HVC "Indicators + Shutdown
// Status" CAN frame (0x134). true => that error is currently asserted by HVC.
bool getAmsError(void);
bool getImdError(void);

#endif // CHARGING_H
