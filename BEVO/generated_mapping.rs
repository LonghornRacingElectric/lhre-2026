// THIS FILE IS AUTO-GENERATED. DO NOT EDIT.
use crate::proto::orion::OrionSensorData;
use crate::config::ProtobufMapping;

pub fn update_proto_field_generated(data: &mut OrionSensorData, name: &str, val: f32, config: &ProtobufMapping) {
    let _d = data.dynamics.get_or_insert_with(Default::default);
    let _c = data.controls.get_or_insert_with(Default::default);
    let _p = data.pack.get_or_insert_with(Default::default);
    let _l = data.diagnostics_low.get_or_insert_with(Default::default);
    let _t = data.thermal.get_or_insert_with(Default::default);

    if config.repeated {
        if let Some(i) = config.field_index {
            match name {
                "bl_sprung_accel" => crate::set_vec_index(&mut _d.bl_sprung_accel, i, val),
                "bl_unsprung_accel" => crate::set_vec_index(&mut _d.bl_unsprung_accel, i, val),
                "br_sprung_accel" => crate::set_vec_index(&mut _d.br_sprung_accel, i, val),
                "br_unsprung_accel" => crate::set_vec_index(&mut _d.br_unsprung_accel, i, val),
                "cells_temps" => crate::set_vec_index(&mut _t.cells_temps, i, val),
                "cells_v" => crate::set_vec_index(&mut _t.cells_v, i, val),
                "fl_sprung_accel" => crate::set_vec_index(&mut _d.fl_sprung_accel, i, val),
                "fl_unsprung_accel" => crate::set_vec_index(&mut _d.fl_unsprung_accel, i, val),
                "fr_sprung_accel" => crate::set_vec_index(&mut _d.fr_sprung_accel, i, val),
                "fr_unsprung_accel" => crate::set_vec_index(&mut _d.fr_unsprung_accel, i, val),
                _ => (),
            }
        }
    } else {
        match name {
            "ambient_temp" => _t.ambient_temp = val,
            "apps1_travel" => _c.apps1_travel = val,
            "apps1_v" => _c.apps1_v = val,
            "apps2_travel" => _c.apps2_travel = val,
            "apps2_v" => _c.apps2_v = val,
            "batt_loop_batt_temp" => _t.batt_loop_batt_temp = val,
            "batt_loop_rad_fan_speed" => _t.batt_loop_rad_fan_speed = val,
            "batt_loop_rad_temp" => _t.batt_loop_rad_temp = val,
            "bl_ride_height" => _d.bl_ride_height = val,
            "bl_strain_gauge_v" => _d.bl_strain_gauge_v = val,
            "bl_sus_pot_v" => _d.bl_sus_pot_v = val,
            "blw_speed" => _d.blw_speed = val,
            "bmb_comm_error" => _l.bmb_comm_error = val != 0.0,
            "bpps1_travel" => _c.bpps1_travel = val,
            "bpps1_v" => _c.bpps1_v = val,
            "bpps2_travel" => _c.bpps2_travel = val,
            "bpps2_v" => _c.bpps2_v = val,
            "br_ride_height" => _d.br_ride_height = val,
            "br_strain_gauge_v" => _d.br_strain_gauge_v = val,
            "br_sus_pot_v" => _d.br_sus_pot_v = val,
            "brake_bias" => _c.brake_bias = val,
            "brake_pressure_f" => _c.brake_pressure_f = val,
            "brake_pressure_rall" => _c.brake_pressure_rall = val,
            "brake_pressure_rbll" => _c.brake_pressure_rbll = val,
            "brw_speed" => _d.brw_speed = val,
            "bse1_v" => _c.bse1_v = val,
            "bse2_v" => _c.bse2_v = val,
            "bse3_v" => _c.bse3_v = val,
            "bus_bar_temp1" => _t.bus_bar_temp1 = val,
            "bus_bar_temp2" => _t.bus_bar_temp2 = val,
            "bus_bar_temp3" => _t.bus_bar_temp3 = val,
            "dc_bus_v" => _p.dc_bus_v = val,
            "discharge_r_temp" => _t.discharge_r_temp = val,
            "fl_ride_height" => _d.fl_ride_height = val,
            "fl_strain_gauge_v" => _d.fl_strain_gauge_v = val,
            "fl_sus_pot_v" => _d.fl_sus_pot_v = val,
            "flw_speed" => _d.flw_speed = val,
            "fr_ride_height" => _d.fr_ride_height = val,
            "fr_strain_gauge_v" => _d.fr_strain_gauge_v = val,
            "fr_sus_pot_v" => _d.fr_sus_pot_v = val,
            "frw_speed" => _d.frw_speed = val,
            "hv_c" => _p.hv_c = val,
            "hv_pack_v" => _p.hv_pack_v = val,
            "hv_soc" => _p.hv_soc = val,
            "imd_gnd_isolation_error" => _l.imd_gnd_isolation_error = val != 0.0,
            "inverter_temp" => _t.inverter_temp = val,
            "lv_batt_c" => _p.lv_batt_c = val,
            "lv_batt_t" => _p.lv_batt_t = val,
            "lv_batt_v" => _p.lv_batt_v = val,
            "motor_loop_inverter_temp" => _t.motor_loop_inverter_temp = val,
            "motor_loop_motor_temp" => _t.motor_loop_motor_temp = val,
            "motor_loop_rad_fan_speed" => _t.motor_loop_rad_fan_speed = val,
            "motor_loop_rad_temp" => _t.motor_loop_rad_temp = val,
            "motor_temp" => _t.motor_temp = val,
            "precharge_r_temp" => _l.precharge_r_temp = val,
            "rpm_request" => _c.rpm_request = val,
            "shutdown_leg1" => _l.shutdown_leg1 = val != 0.0,
            "shutdown_leg2" => _l.shutdown_leg2 = val != 0.0,
            "shutdown_leg3" => _l.shutdown_leg3 = val != 0.0,
            "shutdown_leg4" => _l.shutdown_leg4 = val != 0.0,
            "steer_col_angle" => _d.steer_col_angle = val,
            "torque_command" => _p.torque_command = val,
            "torque_request" => _c.torque_request = val,
            _ => (),
        }
    }
}

pub fn update_proto_bool_generated(data: &mut OrionSensorData, name: &str, val: bool) {
    match name {
        _ => (),
    }
}