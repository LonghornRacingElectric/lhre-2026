#include "vcu_model/inc/vcu_inputs.h"
#include "vcu_model/inc/vcu_outputs.h"
#include "vcu_model/inc/vcu_parameters.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "vcu_model.h"

namespace py = pybind11;

PYBIND11_MODULE(vcu_model_sim, m) {
  m.doc() = "VCU model simulation bindings";

  // ── vcu_inputs_t ──────────────────────────────────────────────────────
  py::class_<vcu_inputs_t>(m, "VcuInputs")
      .def(py::init<>())
      .def_readwrite("apps1_raw", &vcu_inputs_t::apps1_raw)
      .def_readwrite("apps2_raw", &vcu_inputs_t::apps2_raw)
      .def_readwrite("bse1_raw", &vcu_inputs_t::bse1_raw)
      .def_readwrite("bse2_raw", &vcu_inputs_t::bse2_raw)
      .def_readwrite("drive_switch", &vcu_inputs_t::drive_switch)
      .def_readwrite("contactors_closed", &vcu_inputs_t::contactors_closed)
      .def_readwrite("motor_speed_rpm", &vcu_inputs_t::motor_speed_rpm)
      .def_readwrite("min_cell_voltage_v", &vcu_inputs_t::min_cell_voltage_v)
      .def_readwrite("max_cell_voltage_v", &vcu_inputs_t::max_cell_voltage_v)
      .def_readwrite("battery_voltage_v", &vcu_inputs_t::battery_voltage_v)
      .def_readwrite("battery_current_a", &vcu_inputs_t::battery_current_a)
      .def_readwrite("battery_soc_pct", &vcu_inputs_t::battery_soc_pct)
      .def_readwrite("min_cell_temp_c", &vcu_inputs_t::min_cell_temp_c)
      .def_readwrite("max_cell_temp_c", &vcu_inputs_t::max_cell_temp_c)
      .def_readwrite("motor_speed_valid", &vcu_inputs_t::motor_speed_valid)
      .def_readwrite("inverter_voltage_valid",
                     &vcu_inputs_t::inverter_voltage_valid)
      .def_readwrite("inverter_current_valid",
                     &vcu_inputs_t::inverter_current_valid)
      .def_readwrite("battery_pack_status_valid",
                     &vcu_inputs_t::battery_pack_status_valid);

  // ── vcu_outputs_t (nested faults / debug) ─────────────────────────────
  py::class_<vcu_outputs_t> outputs(m, "VcuOutputs");
  outputs
      .def(py::init<>())
      // Normalized pedal travel
      .def_readwrite("apps1_travel", &vcu_outputs_t::apps1_travel)
      .def_readwrite("apps2_travel", &vcu_outputs_t::apps2_travel)
      .def_readwrite("accel_pedal_travel", &vcu_outputs_t::accel_pedal_travel)
      // Torque command
      .def_readwrite("torque_cmd", &vcu_outputs_t::torque_cmd)
      .def_readwrite("linelock_enabled", &vcu_outputs_t::linelock_enabled)
      .def_readwrite("regen_available", &vcu_outputs_t::regen_available)
      .def_readwrite("regen_pressure_requested_torque_nm",
                     &vcu_outputs_t::regen_pressure_requested_torque_nm)
      .def_readwrite("regen_torque_limit_nm",
                     &vcu_outputs_t::regen_torque_limit_nm)
      .def_readwrite("regen_torque_cmd_nm",
                     &vcu_outputs_t::regen_torque_cmd_nm)
      .def_readwrite("regen_pack_current_limit_a",
                     &vcu_outputs_t::regen_pack_current_limit_a)
      .def_readwrite("regen_measured_pack_current_a",
                     &vcu_outputs_t::regen_measured_pack_current_a)
      .def_readwrite("regen_estimated_pack_ocv_v",
                     &vcu_outputs_t::regen_estimated_pack_ocv_v)
      .def_readwrite("max_open_circuit_cell_voltage",
                     &vcu_outputs_t::max_open_circuit_cell_voltage)
      .def_readwrite("inverter_enable", &vcu_outputs_t::inverter_enable)
      // Status flags
      .def_readwrite("brake_pressed", &vcu_outputs_t::brake_pressed)
      .def_readwrite("brake_light_pct", &vcu_outputs_t::brake_light_pct)
      .def_readwrite("prndl_state", &vcu_outputs_t::prndl_state)
      // Buzzer
      .def_readwrite("buzzer_active", &vcu_outputs_t::buzzer_active)
      // BSE pressure
      .def_readwrite("bse1_psi", &vcu_outputs_t::bse1_psi)
      .def_readwrite("bse2_psi", &vcu_outputs_t::bse2_psi)
      .def_readwrite("bse_psi", &vcu_outputs_t::bse_psi)
      .def_readwrite("bse_psi_filtered", &vcu_outputs_t::bse_psi_filtered)
      // Cooling
      .def_readwrite("pumps_on", &vcu_outputs_t::pumps_on)
      .def_readwrite("rad_fans_pct", &vcu_outputs_t::rad_fans_pct)
      .def_readwrite("bat_fans_pct", &vcu_outputs_t::bat_fans_pct)
      // Nested structs (read-only references)
      .def_readonly("faults", &vcu_outputs_t::faults)
      .def_readonly("debug", &vcu_outputs_t::debug);

  // Anonymous struct workaround: declare the nested types through the parent
  // We use decltype to reference the anonymous struct types.
  using faults_t = decltype(vcu_outputs_t::faults);
  py::class_<faults_t>(outputs, "Faults")
      .def(py::init<>())
      .def_readwrite("apps1_under_range", &faults_t::apps1_under_range)
      .def_readwrite("apps1_over_range", &faults_t::apps1_over_range)
      .def_readwrite("apps2_under_range", &faults_t::apps2_under_range)
      .def_readwrite("apps2_over_range", &faults_t::apps2_over_range)
      .def_readwrite("apps_implaus", &faults_t::apps_implaus)
      .def_readwrite("apps_any_fault", &faults_t::apps_any_fault)
      .def_readwrite("bse1_under_range", &faults_t::bse1_under_range)
      .def_readwrite("bse1_over_range", &faults_t::bse1_over_range)
      .def_readwrite("bse2_under_range", &faults_t::bse2_under_range)
      .def_readwrite("bse2_over_range", &faults_t::bse2_over_range)
      .def_readwrite("brake_latched", &faults_t::brake_latched)
      .def_readwrite("brake_any_fault", &faults_t::brake_any_fault)
      .def_readwrite("regen_linelock_input_invalid",
                     &faults_t::regen_linelock_input_invalid)
      .def_readwrite("regen_linelock_ocv_too_high",
                     &faults_t::regen_linelock_ocv_too_high)
      .def_readwrite("regen_linelock_pack_temp_low",
                     &faults_t::regen_linelock_pack_temp_low)
      .def_readwrite("regen_linelock_pack_temp_high",
                     &faults_t::regen_linelock_pack_temp_high)
      .def_readwrite("regen_linelock_motor_speed_low",
                     &faults_t::regen_linelock_motor_speed_low)
      .def_readwrite("regen_linelock_current_hard_cut",
                     &faults_t::regen_linelock_current_hard_cut)
      .def_readwrite("regen_linelock_any_fault",
                     &faults_t::regen_linelock_any_fault)
      .def_readwrite("any_fault", &faults_t::any_fault);

  using debug_t = decltype(vcu_outputs_t::debug);
  py::class_<debug_t>(outputs, "Debug")
      .def(py::init<>())
      .def_readwrite("apps_implaus_ms", &debug_t::apps_implaus_ms)
      .def_readwrite("apps_diff", &debug_t::apps_diff)
      .def_readwrite("apps1_travel", &debug_t::apps1_travel)
      .def_readwrite("apps2_travel", &debug_t::apps2_travel);

  // ── vcu_parameters_t (nested apps / bse / torque_map) ─────────────────
  py::class_<vcu_parameters_t> params(m, "VcuParameters");
  params.def(py::init<>())
      .def_readwrite("brake_enable_threshold",
                     &vcu_parameters_t::brake_enable_threshold)
      .def_readwrite("buzzer_duration_ms",
                     &vcu_parameters_t::buzzer_duration_ms)
      .def_readwrite("apps", &vcu_parameters_t::apps)
      .def_readwrite("bse", &vcu_parameters_t::bse)
      .def_readwrite("torque_map", &vcu_parameters_t::torque_map)
      .def_readwrite("regen_linelock", &vcu_parameters_t::regen_linelock);

  using apps_params_t = decltype(vcu_parameters_t::apps);
  py::class_<apps_params_t>(params, "AppsParams")
      .def(py::init<>())
      .def_readwrite("apps1_min_adc_v", &apps_params_t::apps1_min_adc_v)
      .def_readwrite("apps1_max_adc_v", &apps_params_t::apps1_max_adc_v)
      .def_readwrite("apps2_min_adc_v", &apps_params_t::apps2_min_adc_v)
      .def_readwrite("apps2_max_adc_v", &apps_params_t::apps2_max_adc_v)
      .def_readwrite("min_travel_threshold",
                     &apps_params_t::min_travel_threshold)
      .def_readwrite("max_travel_restore_threshold",
                     &apps_params_t::max_travel_restore_threshold)
      .def_readwrite("max_allowable_diff", &apps_params_t::max_allowable_diff)
      .def_readwrite("max_travel_deadzone", &apps_params_t::max_travel_deadzone)
      .def_readwrite("min_travel_deadzone", &apps_params_t::min_travel_deadzone)
      .def_readwrite("implaus_debounce_time_ms",
                     &apps_params_t::implaus_debounce_time_ms)
      .def_readwrite("pedal_ema_alpha", &apps_params_t::pedal_ema_alpha);

  using bse_params_t = decltype(vcu_parameters_t::bse);
  py::class_<bse_params_t>(params, "BseParams")
      .def(py::init<>())
      .def_readwrite("bse_off_psi", &bse_params_t::bse_off_psi)
      .def_readwrite("bse_on_psi", &bse_params_t::bse_on_psi)
      .def_readwrite("bse1_adc_at_min_psi_v",
                     &bse_params_t::bse1_adc_at_min_psi_v)
      .def_readwrite("bse1_adc_at_max_psi_v",
                     &bse_params_t::bse1_adc_at_max_psi_v)
      .def_readwrite("bse2_adc_at_min_psi_v",
                     &bse_params_t::bse2_adc_at_min_psi_v)
      .def_readwrite("bse2_adc_at_max_psi_v",
                     &bse_params_t::bse2_adc_at_max_psi_v)
      .def_readwrite("bse_max_psi", &bse_params_t::bse_max_psi)
      .def_readwrite("max_pedal_while_braking",
                     &bse_params_t::max_pedal_while_braking)
      .def_readwrite("max_pedal_restore_threshold",
                     &bse_params_t::max_pedal_restore_threshold)
      .def_readwrite("min_psi_deadzone", &bse_params_t::min_psi_deadzone)
      .def_readwrite("max_psi_deadzone", &bse_params_t::max_psi_deadzone)
      .def_readwrite("bse_ema_alpha", &bse_params_t::bse_ema_alpha)
      .def_readwrite("brake_light_min_pct", &bse_params_t::brake_light_min_pct)
      .def_readwrite("brake_light_max_pct", &bse_params_t::brake_light_max_pct);

//   using torque_map_params_t = decltype(vcu_parameters_t::torque_map);
//   py::class_<torque_map_params_t>(params, "TorqueMapParams")
//       .def(py::init<>())
//       .def_readwrite("max_torque_nm", &torque_map_params_t::max_torque_nm);

  using regen_linelock_params_t = decltype(vcu_parameters_t::regen_linelock);
  py::class_<regen_linelock_params_t>(params, "RegenLinelockParams")
      .def(py::init<>())
      .def_readwrite("disable", &regen_linelock_params_t::disable)
      .def_readwrite("pressure_only_test_mode",
                     &regen_linelock_params_t::pressure_only_test_mode)
      .def_readwrite("dc_bus_current_regen_is_negative",
                     &regen_linelock_params_t::dc_bus_current_regen_is_negative)
      .def_readwrite("rear_pressure_zero_torque_psi",
                     &regen_linelock_params_t::rear_pressure_zero_torque_psi)
      .def_readwrite("rear_pressure_reference_psi",
                     &regen_linelock_params_t::rear_pressure_reference_psi)
      .def_readwrite("rear_pressure_min_engage_psi",
                     &regen_linelock_params_t::rear_pressure_min_engage_psi)
      .def_readwrite("regen_torque_at_reference_pressure_nm",
                     &regen_linelock_params_t::
                         regen_torque_at_reference_pressure_nm)
      .def_readwrite("absolute_regen_torque_cap_nm",
                     &regen_linelock_params_t::absolute_regen_torque_cap_nm)
      .def_readwrite("pedal_torque_release_threshold_nm",
                     &regen_linelock_params_t::
                         pedal_torque_release_threshold_nm)
      .def_readwrite("pack_current_limit_a",
                     &regen_linelock_params_t::pack_current_limit_a)
      .def_readwrite("hard_cut_margin_pct",
                     &regen_linelock_params_t::hard_cut_margin_pct)
      .def_readwrite("hard_cut_reset_pressure_psi",
                     &regen_linelock_params_t::hard_cut_reset_pressure_psi)
      .def_readwrite("pack_terminal_voltage_limit_v",
                     &regen_linelock_params_t::pack_terminal_voltage_limit_v)
      .def_readwrite("pack_resistance_ohm",
                     &regen_linelock_params_t::pack_resistance_ohm)
      .def_readwrite("pack_series_cell_count",
                     &regen_linelock_params_t::pack_series_cell_count)
      .def_readwrite("dynamic_voltage_reserve_v",
                     &regen_linelock_params_t::dynamic_voltage_reserve_v)
      .def_readwrite("pack_ocv_enable_v",
                     &regen_linelock_params_t::pack_ocv_enable_v)
      .def_readwrite("pack_ocv_disable_hysteresis_v",
                     &regen_linelock_params_t::
                         pack_ocv_disable_hysteresis_v)
      .def_readwrite("min_cell_temp_c",
                     &regen_linelock_params_t::min_cell_temp_c)
      .def_readwrite("max_cell_temp_c",
                     &regen_linelock_params_t::max_cell_temp_c)
      .def_readwrite("min_motor_speed_rpm",
                     &regen_linelock_params_t::min_motor_speed_rpm);

  // ── vcu_model_context_t ───────────────────────────────────────────────
  py::class_<vcu_model_context_t>(m, "VcuModelContext")
      .def(py::init<>())
      .def_readwrite("time_ms", &vcu_model_context_t::time_ms)
      .def_readwrite("params", &vcu_model_context_t::params);

  // ── Free functions ────────────────────────────────────────────────────
  m.def(
      "vcu_model_init",
      [](vcu_model_context_t &ctx, const vcu_parameters_t &params) {
        vcu_model_init(&ctx, &params);
      },
      py::arg("ctx"), py::arg("params"),
      "Initialize the VCU model context with the given parameters.");

  m.def(
      "vcu_model_step",
      [](vcu_model_context_t &ctx, const vcu_inputs_t &in, uint32_t dt_ms) {
        vcu_outputs_t out{};
        vcu_model_step(&ctx, &in, &out, dt_ms);
        return out;
      },
      py::arg("ctx"), py::arg("inputs"), py::arg("dt_ms"),
      "Run one control step and return the outputs.");
}
