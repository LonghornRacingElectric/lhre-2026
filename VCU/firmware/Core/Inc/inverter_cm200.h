#ifndef INVERTER_CM200_H
#define INVERTER_CM200_H

#include "main.h"
#include "fdcan.h"
#include <stdint.h>
#include <stdbool.h>

// Initialize CM200 inverter interface on the given FDCAN handle.
// This will start the FDCAN peripheral and configure RX filters.
void cm200_init(FDCAN_HandleTypeDef *hfdcan);

// Send a torque command to the inverter.
// torque_nm is in Nm (0.1 Nm resolution in the CAN frame).
void cm200_send_torque(float torque_nm);

// Process any pending inverter CAN messages (0x0B0, 0x0AC).
// Safe to call periodically from a task.
void cm200_process_rx(void);

#endif // INVERTER_CM200_H
