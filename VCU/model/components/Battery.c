#include "Battery.h"
#include "Lookup1D.h"
#include "util.h"

static Lookup1D soe_from_cell_voltage_lookup;

void battery_init(battery_state_t *state, const vcu_parameters_t *params) {
  ema_filter_init(&state->cell_voltage_filter);

  Lookup1D_init(&soe_from_cell_voltage_lookup, params->battery.min_soe_cell_voltage,
                params->battery.max_soe_cell_voltage, params->battery.soe_from_cell_voltage);
}

void battery_evaluate(const vcu_inputs_t *in, vcu_outputs_t *out,
                      battery_state_t *state, const vcu_parameters_t *params,
                      uint32_t dt_ms) {
  (void)dt_ms;

  static float remembered_open_circuit_cell_voltage = 0.0f;

  if(in->battery_current_a > -1.0f && in->battery_current_a < 1.0f) {
    out->open_circuit_cell_voltage = ema_filter_evaluate(
        &state->cell_voltage_filter, in->min_cell_voltage_v,
        params->battery.cell_voltage_ema_alpha);
    remembered_open_circuit_cell_voltage = out->open_circuit_cell_voltage;
  } else {
    out->open_circuit_cell_voltage = remembered_open_circuit_cell_voltage;
  }

  out->soe_pct = Lookup1D_evaluate(&soe_from_cell_voltage_lookup,
                                   in->min_cell_voltage_v);
}
