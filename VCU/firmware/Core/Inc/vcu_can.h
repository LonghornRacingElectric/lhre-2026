#ifndef VCU_CAN_H
#define VCU_CAN_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize CAN library + FDCAN1 interface
void vcu_can_init(void);

// Periodic service; call often (already do this in StartTorqueTask)
void vcu_can_service(void);

// Set inverter torque command (CM200 0x0C0)
//
// torque_nm       – desired torque in Nm (will be clamped to [0, torque_limit_nm])
// torque_limit_nm – limit in Nm (also encoded into the frame)
// enable          – inverter enable flag
// direction       – 0 = reverse, 1 = forward (passed straight into CAN byte)
void vcu_can_set_torque(float torque_nm,
                        float torque_limit_nm,
                        bool enable,
                        uint8_t direction);

// Brake light command hook
void vcu_can_set_brake_light(bool brake_active);

#ifdef __cplusplus
}
#endif

#endif  // VCU_CAN_H
