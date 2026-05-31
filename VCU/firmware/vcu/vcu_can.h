#ifndef VCU_CAN_H
#define VCU_CAN_H

#include "vcu_inputs.h"
#include "vcu_outputs.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


void vcu_can_init(void);

// Send torque request in Nm
void vcu_can_set_torque(float torque_nm);

// Clear all inverter faults (parameter 20, write 0)
void vcu_can_clear_inverter_faults(void);

// Read inverter feedback
void vcu_can_read_feedback(void);

// Read contactor status from CAN messages
void vcu_can_read_contactor_status(void);

void vcu_can_set_model_inputs(const vcu_inputs_t *in);
void vcu_can_set_model_outputs(const vcu_outputs_t *out);
void vcu_can_set_steering_angle_deg(float steering_angle_deg);

bool is_drive_switch_pressed(void);

bool hvc_tractive_ready(void);

float vcu_can_get_motor_speed_rpm(void);
float vcu_can_get_delta_resolver_angle_deg(void);
float vcu_can_get_motor_angle_deg(void);
float vcu_can_get_min_cell_voltage_v(void);

typedef struct {
    float phase_a;
    float phase_b;
    float phase_c;
    float dc_bus;
} inverter_currents_t;

typedef struct {
    float dc_bus;
    float neutral_output;
    float vab_vq;
    float vbc_vd;
} inverter_voltages_t;

inverter_currents_t vcu_can_get_inverter_currents(void);
inverter_voltages_t vcu_can_get_inverter_voltages(void);

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
