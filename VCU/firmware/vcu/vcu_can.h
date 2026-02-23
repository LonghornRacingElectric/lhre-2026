#ifndef VCU_CAN_H
#define VCU_CAN_H

#include "vcu_outputs.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void vcu_can_init(void);

// Send torque request in Nm
void vcu_can_set_torque(float torque_nm);

// Read inverter feedback
void vcu_can_read_feedback(void);

// Read contactor status from CAN messages
void vcu_can_read_contactor_status(void);

void vcu_can_set_model_outputs(vcu_outputs_t *out);

bool is_drive_switch_pressed(void);

bool hvc_tractive_ready(void);

// Inverter feedback variables (populated automatically)
extern float inverter_torque_fb;   // Nm
extern int16_t inverter_rpm;       // rpm
extern float inverter_bus_voltage; // V

// Derived HV state
extern bool hv_contactors_closed;
extern int hvc_state;

#ifdef __cplusplus
}
#endif

#endif
