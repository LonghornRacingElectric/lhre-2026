#ifndef VCU_CAN_H
#define VCU_CAN_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void vcu_can_init(void);
void vcu_can_service(void);

// Send torque request in Nm
void vcu_can_set_torque(float torque_nm);

// Inverter feedback variables (populated automatically)
extern float   inverter_torque_fb;   // Nm
extern int16_t inverter_rpm;         // rpm
extern float   inverter_bus_voltage; // V

#ifdef __cplusplus
}
#endif

#endif
