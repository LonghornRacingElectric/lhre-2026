#include "RegenLinelock.h"

#include "util.h"

#include <math.h>

#define PI_F 3.14159265358979323846f

static bool regen_linelock_params_valid(const vcu_parameters_t *params) {
  return params->regen_linelock.pack_current_limit_a > 0.0f &&
         params->regen_linelock.pack_terminal_voltage_limit_v > 0.0f &&
         params->regen_linelock.pack_ocv_enable_v > 0.0f &&
         params->regen_linelock.pack_series_cell_count > 0.0f &&
         params->regen_linelock.rear_pressure_reference_psi >
             params->regen_linelock.rear_pressure_zero_torque_psi &&
         params->regen_linelock.regen_torque_at_reference_pressure_nm > 0.0f &&
         params->regen_linelock.pedal_torque_open_pulse_threshold_nm >= 0.0f &&
         params->regen_linelock.linelock_open_pulse_ms > 0u &&
         params->regen_linelock.linelock_close_delay_ms > 0u;
}

static float
regen_linelock_pressure_request_nm(float rear_pressure_psi,
                                   const vcu_parameters_t *params) {
  const float zero_psi = params->regen_linelock.rear_pressure_zero_torque_psi;
  const float ref_psi = params->regen_linelock.rear_pressure_reference_psi;
  const float ref_torque =
      params->regen_linelock.regen_torque_at_reference_pressure_nm;

  if (rear_pressure_psi <= zero_psi || ref_psi <= zero_psi ||
      ref_torque <= 0.0f) {
    return 0.0f;
  }

  return (rear_pressure_psi - zero_psi) * ref_torque / (ref_psi - zero_psi);
}

static void regen_linelock_update_ocv_latch(regen_linelock_state_t *state,
                                            float pack_ocv_v,
                                            const vcu_parameters_t *params) {
  const float enable_v = params->regen_linelock.pack_ocv_enable_v;
  const float disable_v =
      enable_v + params->regen_linelock.pack_ocv_disable_hysteresis_v;

  if (enable_v <= 0.0f || pack_ocv_v <= 0.0f) {
    state->ocv_regen_allowed = false;
    return;
  }

  if (!state->ocv_regen_allowed && pack_ocv_v <= enable_v) {
    state->ocv_regen_allowed = true;
  } else if (state->ocv_regen_allowed && pack_ocv_v >= disable_v) {
    state->ocv_regen_allowed = false;
  }
}

void regen_linelock_init(regen_linelock_state_t *state,
                         const vcu_parameters_t *params) {
  (void)params;
  state->ocv_regen_allowed = false;
  state->current_hard_cut_latched = false;
  state->linelock_commanded_closed_ms = 0u;
  state->linelock_open_pulse_remaining_ms = 0u;
  state->previous_pedal_torque_cmd_nm = 0.0f;
}

static bool regen_linelock_close_delay_elapsed(regen_linelock_state_t *state,
                                               bool command_linelock,
                                               uint32_t dt_ms,
                                               const vcu_parameters_t *params) {
  if (!command_linelock) {
    state->linelock_commanded_closed_ms = 0u;
    return false;
  }

  const uint32_t delay_ms = params->regen_linelock.linelock_close_delay_ms;
  if (state->linelock_commanded_closed_ms < delay_ms) {
    const uint32_t remaining_ms =
        delay_ms - state->linelock_commanded_closed_ms;
    state->linelock_commanded_closed_ms =
        dt_ms >= remaining_ms ? delay_ms
                              : state->linelock_commanded_closed_ms + dt_ms;
  }

  return state->linelock_commanded_closed_ms >= delay_ms;
}

static void regen_linelock_reset_command_delay(regen_linelock_state_t *state) {
  state->linelock_commanded_closed_ms = 0u;
}

static bool regen_linelock_open_pulse_active(regen_linelock_state_t *state,
                                             float pedal_torque_cmd_nm,
                                             uint32_t dt_ms,
                                             const vcu_parameters_t *params) {
  const float threshold_nm =
      params->regen_linelock.pedal_torque_open_pulse_threshold_nm;
  const bool rising_edge = state->previous_pedal_torque_cmd_nm <= threshold_nm &&
                           pedal_torque_cmd_nm > threshold_nm;

  if (rising_edge) {
    state->linelock_open_pulse_remaining_ms =
        params->regen_linelock.linelock_open_pulse_ms;
  }

  const bool active = state->linelock_open_pulse_remaining_ms > 0u;
  if (active) {
    state->linelock_open_pulse_remaining_ms =
        dt_ms >= state->linelock_open_pulse_remaining_ms
            ? 0u
            : state->linelock_open_pulse_remaining_ms - dt_ms;
  }

  state->previous_pedal_torque_cmd_nm = pedal_torque_cmd_nm;
  return active;
}

float regen_linelock_measured_pack_regen_current_a(
    float dc_bus_current_a, const vcu_parameters_t *params) {
  float regen_current_a =
      params->regen_linelock.dc_bus_current_regen_is_negative
          ? -dc_bus_current_a
          : dc_bus_current_a;
  return regen_current_a > 0.0f ? regen_current_a : 0.0f;
}

float regen_linelock_estimated_pack_ocv_v(float terminal_voltage_v,
                                          float dc_bus_current_a,
                                          const vcu_parameters_t *params) {
  const float regen_current_a =
      regen_linelock_measured_pack_regen_current_a(dc_bus_current_a, params);
  const float resistance_ohm = params->regen_linelock.pack_resistance_ohm;

  if (terminal_voltage_v <= 0.0f) {
    return 0.0f;
  }
  if (resistance_ohm <= 0.0f) {
    return terminal_voltage_v;
  }

  const float ocv_v = terminal_voltage_v - regen_current_a * resistance_ohm;
  return ocv_v > 0.0f ? ocv_v : 0.0f;
}

float regen_linelock_available_pack_current_a(float pack_ocv_v,
                                              const vcu_parameters_t *params) {
  const float pack_current_limit_a =
      params->regen_linelock.pack_current_limit_a;
  const float resistance_ohm = params->regen_linelock.pack_resistance_ohm;
  const float headroom_v =
      params->regen_linelock.pack_terminal_voltage_limit_v - pack_ocv_v -
      params->regen_linelock.dynamic_voltage_reserve_v;

  if (pack_current_limit_a <= 0.0f || pack_ocv_v <= 0.0f ||
      params->regen_linelock.pack_terminal_voltage_limit_v <= 0.0f) {
    return 0.0f;
  }

  const float voltage_limited_current_a =
      resistance_ohm > 0.0f ? headroom_v / resistance_ohm
                            : (headroom_v > 0.0f ? pack_current_limit_a : 0.0f);

  return clamp_f(voltage_limited_current_a, 0.0f, pack_current_limit_a);
}

float regen_linelock_torque_limit_nm(float motor_speed_rpm, float pack_ocv_v,
                                     const vcu_parameters_t *params) {
  const float pack_current_a =
      regen_linelock_available_pack_current_a(pack_ocv_v, params);
  const float resistance_ohm = params->regen_linelock.pack_resistance_ohm;
  const float omega_rad_s = fabsf(motor_speed_rpm) * 2.0f * PI_F / 60.0f;

  if (pack_current_a <= 0.0f || omega_rad_s <= 0.001f) {
    return 0.0f;
  }

  float terminal_voltage_v = pack_ocv_v + pack_current_a * resistance_ohm;
  if (terminal_voltage_v >
      params->regen_linelock.pack_terminal_voltage_limit_v) {
    terminal_voltage_v = params->regen_linelock.pack_terminal_voltage_limit_v;
  }

  float torque_nm = terminal_voltage_v * pack_current_a / omega_rad_s;
  const float cap_nm = params->regen_linelock.absolute_regen_torque_cap_nm;
  if (cap_nm > 0.0f) {
    torque_nm = clamp_f(torque_nm, 0.0f, cap_nm);
  }
  return torque_nm;
}

void regen_linelock_evaluate(const vcu_inputs_t *in, vcu_outputs_t *out,
                             regen_linelock_state_t *state,
                             const vcu_parameters_t *params, uint32_t dt_ms) {
  const float rear_pressure_psi = out->bse2_psi;
  const float motor_speed_rpm = fabsf(in->motor_speed_rpm);
  const float measured_regen_current_a =
      regen_linelock_measured_pack_regen_current_a(in->battery_current_a,
                                                   params);
  const float pack_ocv_v = out->max_open_circuit_cell_voltage *
                           params->regen_linelock.pack_series_cell_count;
  const bool live_max_cell_voltage_valid =
      in->max_cell_voltage_v > 0.0f &&
      params->regen_linelock.pack_series_cell_count > 0.0f;
  const float live_max_cell_pack_voltage_v =
      live_max_cell_voltage_valid
          ? in->max_cell_voltage_v *
                params->regen_linelock.pack_series_cell_count
          : 0.0f;
  const float voltage_limit_pack_ocv_v =
      live_max_cell_pack_voltage_v > pack_ocv_v ? live_max_cell_pack_voltage_v
                                                : pack_ocv_v;
  const bool live_max_cell_voltage_high =
      params->regen_linelock.max_cell_voltage_regen_disable_v > 0.0f &&
      live_max_cell_voltage_valid &&
      in->max_cell_voltage_v >=
          params->regen_linelock.max_cell_voltage_regen_disable_v;

  out->regen_measured_pack_current_a = measured_regen_current_a;
  out->regen_estimated_pack_ocv_v = pack_ocv_v;
  out->regen_pack_current_limit_a =
      regen_linelock_available_pack_current_a(voltage_limit_pack_ocv_v, params);
  out->regen_pressure_requested_torque_nm =
      regen_linelock_pressure_request_nm(rear_pressure_psi, params);
  out->regen_torque_limit_nm = regen_linelock_torque_limit_nm(
      motor_speed_rpm, voltage_limit_pack_ocv_v, params);
  out->linelock_enabled = false;
  out->regen_available = false;
  out->regen_torque_cmd_nm = 0.0f;

  const float pressure_only_torque_nm =
      clamp_f(out->regen_pressure_requested_torque_nm, 0.0f,
              params->regen_linelock.absolute_regen_torque_cap_nm);
  const bool regen_pressure_requested =
      rear_pressure_psi >=
          params->regen_linelock.rear_pressure_min_engage_psi &&
      out->regen_pressure_requested_torque_nm > 0.0f;
  const bool pedal_torque_open_pulse_active = regen_linelock_open_pulse_active(
      state, out->torque_cmd, dt_ms, params);

  if (state->current_hard_cut_latched &&
      rear_pressure_psi <= params->regen_linelock.hard_cut_reset_pressure_psi) {
    state->current_hard_cut_latched = false;
  }

  const float hard_cut_current_a =
      params->regen_linelock.pack_current_limit_a *
      (1.0f + params->regen_linelock.hard_cut_margin_pct);
  const bool hard_cut_armed = out->regen_pressure_requested_torque_nm > 0.0f;
  if (!params->regen_linelock.disable && hard_cut_armed &&
      hard_cut_current_a > 0.0f &&
      measured_regen_current_a > hard_cut_current_a) {
    state->current_hard_cut_latched = true;
  }

  if (params->regen_linelock.pressure_only_test_mode) {
    out->faults.regen_linelock_live_cell_voltage_high =
        live_max_cell_voltage_high;
    out->faults.regen_linelock_current_hard_cut =
        state->current_hard_cut_latched;
    out->faults.regen_linelock_any_fault = state->current_hard_cut_latched;
    out->regen_available = !params->regen_linelock.disable &&
                           !state->current_hard_cut_latched &&
                           !pedal_torque_open_pulse_active;

    if (params->regen_linelock.disable || live_max_cell_voltage_high ||
        state->current_hard_cut_latched || pedal_torque_open_pulse_active ||
        pressure_only_torque_nm <= 0.0f) {
      if (state->current_hard_cut_latched) {
        out->torque_cmd = 0.0f;
      }
      out->linelock_enabled = false;
      out->regen_torque_cmd_nm = 0.0f;
      regen_linelock_reset_command_delay(state);
      return;
    }

    out->linelock_enabled = true;
    if (!regen_linelock_close_delay_elapsed(state, true, dt_ms, params)) {
      out->regen_available = false;
      out->regen_torque_cmd_nm = 0.0f;
      return;
    }

    out->regen_available = true;
    out->regen_torque_cmd_nm = -pressure_only_torque_nm;
    out->torque_cmd = out->regen_torque_cmd_nm;
    return;
  }

  regen_linelock_update_ocv_latch(state, pack_ocv_v, params);

  const bool params_valid = regen_linelock_params_valid(params);
  const bool input_valid = params_valid && in->inverter_current_valid &&
                           in->motor_speed_valid && pack_ocv_v > 0.0f;
  const bool temp_low = false;
  const bool temp_high = false;
  const bool motor_speed_low =
      input_valid &&
      motor_speed_rpm < params->regen_linelock.min_motor_speed_rpm;

  out->faults.regen_linelock_input_invalid = !input_valid;
  out->faults.regen_linelock_ocv_too_high = !state->ocv_regen_allowed;
  out->faults.regen_linelock_live_cell_voltage_high =
      live_max_cell_voltage_high;
  out->faults.regen_linelock_pack_temp_low = temp_low;
  out->faults.regen_linelock_pack_temp_high = temp_high;
  out->faults.regen_linelock_motor_speed_low = motor_speed_low;
  out->faults.regen_linelock_current_hard_cut = state->current_hard_cut_latched;
  out->faults.regen_linelock_any_fault = state->current_hard_cut_latched;

  const bool regen_eligible =
      !params->regen_linelock.disable && input_valid &&
      state->ocv_regen_allowed && !live_max_cell_voltage_high && !temp_low &&
      !temp_high && !motor_speed_low && !state->current_hard_cut_latched &&
      !out->faults.apps_any_fault && !pedal_torque_open_pulse_active &&
      regen_pressure_requested;
  const bool linelock_eligible =
      !params->regen_linelock.disable && input_valid &&
      state->ocv_regen_allowed && !live_max_cell_voltage_high && !temp_low &&
      !temp_high && !motor_speed_low && !state->current_hard_cut_latched &&
      !out->faults.apps_any_fault && !pedal_torque_open_pulse_active;

  if (state->current_hard_cut_latched || pedal_torque_open_pulse_active) {
    if (state->current_hard_cut_latched) {
      out->torque_cmd = 0.0f;
    }
    out->linelock_enabled = false;
    out->regen_torque_cmd_nm = 0.0f;
    out->regen_available = false;
    regen_linelock_reset_command_delay(state);
    return;
  }

  const float regen_torque_nm = clamp_f(out->regen_pressure_requested_torque_nm,
                                        0.0f, out->regen_torque_limit_nm);
  const bool command_linelock = linelock_eligible;
  out->linelock_enabled = command_linelock;

  if (!regen_linelock_close_delay_elapsed(state, command_linelock, dt_ms,
                                          params)) {
    out->regen_available = false;
    out->regen_torque_cmd_nm = 0.0f;
    return;
  }

  if (!regen_eligible || regen_torque_nm <= 0.0f) {
    out->regen_available = false;
    out->regen_torque_cmd_nm = 0.0f;
    return;
  }

  out->regen_available = true;
  out->regen_torque_cmd_nm = -regen_torque_nm;
  out->torque_cmd = out->regen_torque_cmd_nm;
}
