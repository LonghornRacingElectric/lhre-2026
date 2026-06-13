#include "Battery.h"
#include "Lookup1D.h"
#include "util.h"

static Lookup1D soe_from_cell_voltage_lookup;

void battery_init(battery_state_t *state, const vcu_parameters_t *params) {
  ema_filter_init(&state->cell_voltage_filter);
  ema_filter_init(&state->max_cell_voltage_filter);

  state->net_energy_wh = 0;
  state->regen_energy_wh = 0;

  Lookup1D_init(&soe_from_cell_voltage_lookup, params->battery.min_soe_cell_voltage,
                params->battery.max_soe_cell_voltage, params->battery.soe_from_cell_voltage);
}

void battery_evaluate(const vcu_inputs_t *in, vcu_outputs_t *out,
                      battery_state_t *state, const vcu_parameters_t *params,
                      uint32_t dt_ms) {
  if(in->battery_current_a > -5.0f && in->battery_current_a < 5.0f) {
    state->current_in_bounds_ms += dt_ms;
  } else {
    state->current_in_bounds_ms = 0;
  }

  if(state->current_in_bounds_ms >= 1000) {
    out->open_circuit_cell_voltage = ema_filter_evaluate(
        &state->cell_voltage_filter, in->min_cell_voltage_v,
        params->battery.cell_voltage_ema_alpha);
    out->max_open_circuit_cell_voltage = ema_filter_evaluate(
        &state->max_cell_voltage_filter, in->max_cell_voltage_v,
        params->battery.cell_voltage_ema_alpha);
    state->remembered_open_circuit_cell_voltage = out->open_circuit_cell_voltage;
    state->remembered_max_open_circuit_cell_voltage =
        out->max_open_circuit_cell_voltage;
  } else {
    out->open_circuit_cell_voltage = state->remembered_open_circuit_cell_voltage;
    out->max_open_circuit_cell_voltage =
        state->remembered_max_open_circuit_cell_voltage;
  }

  out->soe_pct = Lookup1D_evaluate(&soe_from_cell_voltage_lookup,
                                   out->open_circuit_cell_voltage);

  float energy_step_wh = in->battery_current_a * in->battery_voltage_v * (dt_ms / 3600000.0f);
  if(energy_step_wh < 0) {
    state->regen_energy_wh -= energy_step_wh;
  }
  state->net_energy_wh += energy_step_wh;

  if(state->net_energy_wh < 0) {
    state->net_energy_wh = 0;
  }

  out->net_energy_wh = state->net_energy_wh;
  out->regen_energy_wh = state->regen_energy_wh;
}
