// Auto-generated CAN packet implementation file
// Implements functions declared in: can_ids.h
// DO NOT EDIT MANUALLY

#include "longhorn/can/can_ids.h"

// Packet: Firmware Update Command Packet
int pack_firmware_update_command_packet(const msg_firmware_update_command_packet_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, FIRMWARE_UPDATE_COMMAND_PACKET_DLC);

    // Pack: Command
    tx_buf[0] = (uint8_t)msg->command;

    // Pack: Address
    tx_buf[1 + 0] = (uint8_t)(msg->address & 0xFF);
    tx_buf[1 + 1] = (uint8_t)((msg->address >> 8) & 0xFF);
    tx_buf[1 + 2] = (uint8_t)((msg->address >> 16) & 0xFF);
    tx_buf[1 + 3] = (uint8_t)((msg->address >> 24) & 0xFF);

    // Pack: Num Blocks
    tx_buf[5 + 0] = (uint8_t)(msg->num_blocks & 0xFF);
    tx_buf[5 + 1] = (uint8_t)((msg->num_blocks >> 8) & 0xFF);

    // Pack: CRC
    tx_buf[7] = (uint8_t)msg->crc;

    return 0;
}

int unpack_firmware_update_command_packet(const uint8_t* rx_buf, msg_firmware_update_command_packet_t* msg) {
    // Unpack: Command
    msg->command = (uint8_t)rx_buf[0];

    // Unpack: Address
    msg->address = (uint32_t)rx_buf[1 + 0];
    msg->address |= (uint32_t)(rx_buf[1 + 1] << 8);
    msg->address |= (uint32_t)(rx_buf[1 + 2] << 16);
    msg->address |= (uint32_t)(rx_buf[1 + 3] << 24);

    // Unpack: Num Blocks
    msg->num_blocks = (uint16_t)rx_buf[5 + 0];
    msg->num_blocks |= (uint16_t)(rx_buf[5 + 1] << 8);

    // Unpack: CRC
    msg->crc = (uint8_t)rx_buf[7];

    return 0;
}

// Packet: Bus Enable/Disable
int pack_bus_enable_disable(const msg_bus_enable_disable_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, BUS_ENABLE_DISABLE_DLC);

    // Pack: Enable
    tx_buf[0] = (uint8_t)msg->enable;

    // Pack: FW Update
    tx_buf[1] = (uint8_t)msg->fw_update;

    // Pack: Device
    tx_buf[2] = (uint8_t)msg->device;

    return 0;
}

int unpack_bus_enable_disable(const uint8_t* rx_buf, msg_bus_enable_disable_t* msg) {
    // Unpack: Enable
    msg->enable = (uint8_t)rx_buf[0];

    // Unpack: FW Update
    msg->fw_update = (uint8_t)rx_buf[1];

    // Unpack: Device
    msg->device = (uint8_t)rx_buf[2];

    return 0;
}

// Packet: Firmware Update Data Packet
int pack_firmware_update_data_packet(const msg_firmware_update_data_packet_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, FIRMWARE_UPDATE_DATA_PACKET_DLC);

    // Pack: Index
    tx_buf[0] = (uint8_t)msg->index;

    // Pack: data0
    tx_buf[1] = (uint8_t)msg->data0;

    // Pack: data1
    tx_buf[2] = (uint8_t)msg->data1;

    // Pack: data2
    tx_buf[3] = (uint8_t)msg->data2;

    // Pack: data3
    tx_buf[4] = (uint8_t)msg->data3;

    // Pack: data4
    tx_buf[5] = (uint8_t)msg->data4;

    // Pack: data5
    tx_buf[6] = (uint8_t)msg->data5;

    // Pack: data6
    tx_buf[7] = (uint8_t)msg->data6;

    return 0;
}

int unpack_firmware_update_data_packet(const uint8_t* rx_buf, msg_firmware_update_data_packet_t* msg) {
    // Unpack: Index
    msg->index = (uint8_t)rx_buf[0];

    // Unpack: data0
    msg->data0 = (uint8_t)rx_buf[1];

    // Unpack: data1
    msg->data1 = (uint8_t)rx_buf[2];

    // Unpack: data2
    msg->data2 = (uint8_t)rx_buf[3];

    // Unpack: data3
    msg->data3 = (uint8_t)rx_buf[4];

    // Unpack: data4
    msg->data4 = (uint8_t)rx_buf[5];

    // Unpack: data5
    msg->data5 = (uint8_t)rx_buf[6];

    // Unpack: data6
    msg->data6 = (uint8_t)rx_buf[7];

    return 0;
}

// Packet: Device Firmware Update Response Packet
int pack_device_firmware_update_response_packet(const msg_device_firmware_update_response_packet_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, DEVICE_FIRMWARE_UPDATE_RESPONSE_PACKET_DLC);

    // Pack: Response
    tx_buf[0] = (uint8_t)msg->response;

    return 0;
}

int unpack_device_firmware_update_response_packet(const uint8_t* rx_buf, msg_device_firmware_update_response_packet_t* msg) {
    // Unpack: Response
    msg->response = (uint8_t)rx_buf[0];

    return 0;
}

// Packet: Inverter Temps
int pack_inverter_temps(const msg_inverter_temps_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, INVERTER_TEMPS_DLC);

    // Pack: Module A Temp
    int16_t raw_module_a_temp = (int16_t)(msg->module_a_temp / 0.1f);
    tx_buf[0 + 0] = (uint8_t)(raw_module_a_temp & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_module_a_temp >> 8) & 0xFF);

    // Pack: Module B Temp
    int16_t raw_module_b_temp = (int16_t)(msg->module_b_temp / 0.1f);
    tx_buf[2 + 0] = (uint8_t)(raw_module_b_temp & 0xFF);
    tx_buf[2 + 1] = (uint8_t)((raw_module_b_temp >> 8) & 0xFF);

    // Pack: Module C Temp
    int16_t raw_module_c_temp = (int16_t)(msg->module_c_temp / 0.1f);
    tx_buf[4 + 0] = (uint8_t)(raw_module_c_temp & 0xFF);
    tx_buf[4 + 1] = (uint8_t)((raw_module_c_temp >> 8) & 0xFF);

    // Pack: Gate Driver Temp
    int16_t raw_gate_driver_temp = (int16_t)(msg->gate_driver_temp / 0.1f);
    tx_buf[6 + 0] = (uint8_t)(raw_gate_driver_temp & 0xFF);
    tx_buf[6 + 1] = (uint8_t)((raw_gate_driver_temp >> 8) & 0xFF);

    return 0;
}

int unpack_inverter_temps(const uint8_t* rx_buf, msg_inverter_temps_t* msg) {
    // Unpack: Module A Temp
    int16_t raw_module_a_temp = 0;
    raw_module_a_temp = (int16_t)rx_buf[0 + 0];
    raw_module_a_temp |= (int16_t)(rx_buf[0 + 1] << 8);
    msg->module_a_temp = (float)raw_module_a_temp * 0.1f;

    // Unpack: Module B Temp
    int16_t raw_module_b_temp = 0;
    raw_module_b_temp = (int16_t)rx_buf[2 + 0];
    raw_module_b_temp |= (int16_t)(rx_buf[2 + 1] << 8);
    msg->module_b_temp = (float)raw_module_b_temp * 0.1f;

    // Unpack: Module C Temp
    int16_t raw_module_c_temp = 0;
    raw_module_c_temp = (int16_t)rx_buf[4 + 0];
    raw_module_c_temp |= (int16_t)(rx_buf[4 + 1] << 8);
    msg->module_c_temp = (float)raw_module_c_temp * 0.1f;

    // Unpack: Gate Driver Temp
    int16_t raw_gate_driver_temp = 0;
    raw_gate_driver_temp = (int16_t)rx_buf[6 + 0];
    raw_gate_driver_temp |= (int16_t)(rx_buf[6 + 1] << 8);
    msg->gate_driver_temp = (float)raw_gate_driver_temp * 0.1f;

    return 0;
}

// Packet: Inverter Temps 2
int pack_inverter_temps_2(const msg_inverter_temps_2_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, INVERTER_TEMPS_2_DLC);

    // Pack: Coolant Temp
    int16_t raw_coolant_temp = (int16_t)(msg->coolant_temp / 0.1f);
    tx_buf[0 + 0] = (uint8_t)(raw_coolant_temp & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_coolant_temp >> 8) & 0xFF);

    // Pack: Inverter Hot Spot Temp
    int16_t raw_inverter_hot_spot_temp = (int16_t)(msg->inverter_hot_spot_temp / 0.1f);
    tx_buf[2 + 0] = (uint8_t)(raw_inverter_hot_spot_temp & 0xFF);
    tx_buf[2 + 1] = (uint8_t)((raw_inverter_hot_spot_temp >> 8) & 0xFF);

    // Pack: Motor Temp
    int16_t raw_motor_temp = (int16_t)(msg->motor_temp / 0.1f);
    tx_buf[4 + 0] = (uint8_t)(raw_motor_temp & 0xFF);
    tx_buf[4 + 1] = (uint8_t)((raw_motor_temp >> 8) & 0xFF);

    // Pack: Torque Shudder
    int16_t raw_torque_shudder = (int16_t)(msg->torque_shudder / 0.1f);
    tx_buf[6 + 0] = (uint8_t)(raw_torque_shudder & 0xFF);
    tx_buf[6 + 1] = (uint8_t)((raw_torque_shudder >> 8) & 0xFF);

    return 0;
}

int unpack_inverter_temps_2(const uint8_t* rx_buf, msg_inverter_temps_2_t* msg) {
    // Unpack: Coolant Temp
    int16_t raw_coolant_temp = 0;
    raw_coolant_temp = (int16_t)rx_buf[0 + 0];
    raw_coolant_temp |= (int16_t)(rx_buf[0 + 1] << 8);
    msg->coolant_temp = (float)raw_coolant_temp * 0.1f;

    // Unpack: Inverter Hot Spot Temp
    int16_t raw_inverter_hot_spot_temp = 0;
    raw_inverter_hot_spot_temp = (int16_t)rx_buf[2 + 0];
    raw_inverter_hot_spot_temp |= (int16_t)(rx_buf[2 + 1] << 8);
    msg->inverter_hot_spot_temp = (float)raw_inverter_hot_spot_temp * 0.1f;

    // Unpack: Motor Temp
    int16_t raw_motor_temp = 0;
    raw_motor_temp = (int16_t)rx_buf[4 + 0];
    raw_motor_temp |= (int16_t)(rx_buf[4 + 1] << 8);
    msg->motor_temp = (float)raw_motor_temp * 0.1f;

    // Unpack: Torque Shudder
    int16_t raw_torque_shudder = 0;
    raw_torque_shudder = (int16_t)rx_buf[6 + 0];
    raw_torque_shudder |= (int16_t)(rx_buf[6 + 1] << 8);
    msg->torque_shudder = (float)raw_torque_shudder * 0.1f;

    return 0;
}

// Packet: Inverter Status
int pack_inverter_status(const msg_inverter_status_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, INVERTER_STATUS_DLC);

    // Pack: Motor Angle
    int16_t raw_motor_angle = (int16_t)(msg->motor_angle / 0.1f);
    tx_buf[0 + 0] = (uint8_t)(raw_motor_angle & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_motor_angle >> 8) & 0xFF);

    // Pack: Motor Speed
    tx_buf[2 + 0] = (uint8_t)(msg->motor_speed & 0xFF);
    tx_buf[2 + 1] = (uint8_t)((msg->motor_speed >> 8) & 0xFF);

    // Pack: Inverter Frequency
    int16_t raw_inverter_frequency = (int16_t)(msg->inverter_frequency / 0.1f);
    tx_buf[4 + 0] = (uint8_t)(raw_inverter_frequency & 0xFF);
    tx_buf[4 + 1] = (uint8_t)((raw_inverter_frequency >> 8) & 0xFF);

    // Pack: Delta Resolver Angle
    int16_t raw_delta_resolver_angle = (int16_t)(msg->delta_resolver_angle / 0.1f);
    tx_buf[6 + 0] = (uint8_t)(raw_delta_resolver_angle & 0xFF);
    tx_buf[6 + 1] = (uint8_t)((raw_delta_resolver_angle >> 8) & 0xFF);

    return 0;
}

int unpack_inverter_status(const uint8_t* rx_buf, msg_inverter_status_t* msg) {
    // Unpack: Motor Angle
    int16_t raw_motor_angle = 0;
    raw_motor_angle = (int16_t)rx_buf[0 + 0];
    raw_motor_angle |= (int16_t)(rx_buf[0 + 1] << 8);
    msg->motor_angle = (float)raw_motor_angle * 0.1f;

    // Unpack: Motor Speed
    msg->motor_speed = (int16_t)rx_buf[2 + 0];
    msg->motor_speed |= (int16_t)(rx_buf[2 + 1] << 8);

    // Unpack: Inverter Frequency
    int16_t raw_inverter_frequency = 0;
    raw_inverter_frequency = (int16_t)rx_buf[4 + 0];
    raw_inverter_frequency |= (int16_t)(rx_buf[4 + 1] << 8);
    msg->inverter_frequency = (float)raw_inverter_frequency * 0.1f;

    // Unpack: Delta Resolver Angle
    int16_t raw_delta_resolver_angle = 0;
    raw_delta_resolver_angle = (int16_t)rx_buf[6 + 0];
    raw_delta_resolver_angle |= (int16_t)(rx_buf[6 + 1] << 8);
    msg->delta_resolver_angle = (float)raw_delta_resolver_angle * 0.1f;

    return 0;
}

// Packet: Inverter Current
int pack_inverter_current(const msg_inverter_current_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, INVERTER_CURRENT_DLC);

    // Pack: Phase A Current
    int16_t raw_phase_a_current = (int16_t)(msg->phase_a_current / 0.1f);
    tx_buf[0 + 0] = (uint8_t)(raw_phase_a_current & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_phase_a_current >> 8) & 0xFF);

    // Pack: Phase B Current
    int16_t raw_phase_b_current = (int16_t)(msg->phase_b_current / 0.1f);
    tx_buf[2 + 0] = (uint8_t)(raw_phase_b_current & 0xFF);
    tx_buf[2 + 1] = (uint8_t)((raw_phase_b_current >> 8) & 0xFF);

    // Pack: Phase C Current
    int16_t raw_phase_c_current = (int16_t)(msg->phase_c_current / 0.1f);
    tx_buf[4 + 0] = (uint8_t)(raw_phase_c_current & 0xFF);
    tx_buf[4 + 1] = (uint8_t)((raw_phase_c_current >> 8) & 0xFF);

    // Pack: DC Bus Current
    int16_t raw_dc_bus_current = (int16_t)(msg->dc_bus_current / 0.1f);
    tx_buf[6 + 0] = (uint8_t)(raw_dc_bus_current & 0xFF);
    tx_buf[6 + 1] = (uint8_t)((raw_dc_bus_current >> 8) & 0xFF);

    return 0;
}

int unpack_inverter_current(const uint8_t* rx_buf, msg_inverter_current_t* msg) {
    // Unpack: Phase A Current
    int16_t raw_phase_a_current = 0;
    raw_phase_a_current = (int16_t)rx_buf[0 + 0];
    raw_phase_a_current |= (int16_t)(rx_buf[0 + 1] << 8);
    msg->phase_a_current = (float)raw_phase_a_current * 0.1f;

    // Unpack: Phase B Current
    int16_t raw_phase_b_current = 0;
    raw_phase_b_current = (int16_t)rx_buf[2 + 0];
    raw_phase_b_current |= (int16_t)(rx_buf[2 + 1] << 8);
    msg->phase_b_current = (float)raw_phase_b_current * 0.1f;

    // Unpack: Phase C Current
    int16_t raw_phase_c_current = 0;
    raw_phase_c_current = (int16_t)rx_buf[4 + 0];
    raw_phase_c_current |= (int16_t)(rx_buf[4 + 1] << 8);
    msg->phase_c_current = (float)raw_phase_c_current * 0.1f;

    // Unpack: DC Bus Current
    int16_t raw_dc_bus_current = 0;
    raw_dc_bus_current = (int16_t)rx_buf[6 + 0];
    raw_dc_bus_current |= (int16_t)(rx_buf[6 + 1] << 8);
    msg->dc_bus_current = (float)raw_dc_bus_current * 0.1f;

    return 0;
}

// Packet: Inverter Voltage
int pack_inverter_voltage(const msg_inverter_voltage_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, INVERTER_VOLTAGE_DLC);

    // Pack: DC Bus Voltage
    int16_t raw_dc_bus_voltage = (int16_t)(msg->dc_bus_voltage / 0.1f);
    tx_buf[0 + 0] = (uint8_t)(raw_dc_bus_voltage & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_dc_bus_voltage >> 8) & 0xFF);

    // Pack: Neutral Output Voltage
    int16_t raw_neutral_output_voltage = (int16_t)(msg->neutral_output_voltage / 0.1f);
    tx_buf[2 + 0] = (uint8_t)(raw_neutral_output_voltage & 0xFF);
    tx_buf[2 + 1] = (uint8_t)((raw_neutral_output_voltage >> 8) & 0xFF);

    // Pack: Vab / Vq Voltage
    int16_t raw_vab_vq_voltage = (int16_t)(msg->vab_vq_voltage / 0.1f);
    tx_buf[4 + 0] = (uint8_t)(raw_vab_vq_voltage & 0xFF);
    tx_buf[4 + 1] = (uint8_t)((raw_vab_vq_voltage >> 8) & 0xFF);

    // Pack: Vbc / Vd Voltage
    int16_t raw_vbc_vd_voltage = (int16_t)(msg->vbc_vd_voltage / 0.1f);
    tx_buf[6 + 0] = (uint8_t)(raw_vbc_vd_voltage & 0xFF);
    tx_buf[6 + 1] = (uint8_t)((raw_vbc_vd_voltage >> 8) & 0xFF);

    return 0;
}

int unpack_inverter_voltage(const uint8_t* rx_buf, msg_inverter_voltage_t* msg) {
    // Unpack: DC Bus Voltage
    int16_t raw_dc_bus_voltage = 0;
    raw_dc_bus_voltage = (int16_t)rx_buf[0 + 0];
    raw_dc_bus_voltage |= (int16_t)(rx_buf[0 + 1] << 8);
    msg->dc_bus_voltage = (float)raw_dc_bus_voltage * 0.1f;

    // Unpack: Neutral Output Voltage
    int16_t raw_neutral_output_voltage = 0;
    raw_neutral_output_voltage = (int16_t)rx_buf[2 + 0];
    raw_neutral_output_voltage |= (int16_t)(rx_buf[2 + 1] << 8);
    msg->neutral_output_voltage = (float)raw_neutral_output_voltage * 0.1f;

    // Unpack: Vab / Vq Voltage
    int16_t raw_vab_vq_voltage = 0;
    raw_vab_vq_voltage = (int16_t)rx_buf[4 + 0];
    raw_vab_vq_voltage |= (int16_t)(rx_buf[4 + 1] << 8);
    msg->vab_vq_voltage = (float)raw_vab_vq_voltage * 0.1f;

    // Unpack: Vbc / Vd Voltage
    int16_t raw_vbc_vd_voltage = 0;
    raw_vbc_vd_voltage = (int16_t)rx_buf[6 + 0];
    raw_vbc_vd_voltage |= (int16_t)(rx_buf[6 + 1] << 8);
    msg->vbc_vd_voltage = (float)raw_vbc_vd_voltage * 0.1f;

    return 0;
}

// Packet: Inverter Details
int pack_inverter_details(const msg_inverter_details_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, INVERTER_DETAILS_DLC);

    // Pack: VSM
    tx_buf[0] = (uint8_t)msg->vsm;

    // Pack: PWM Freq
    tx_buf[1] = (uint8_t)msg->pwm_freq;

    // Pack: Inverter
    tx_buf[2] = (uint8_t)msg->inverter;

    // Pack: Relay
    tx_buf[3] = (uint8_t)msg->relay;

    // Pack: Misc. 1
    tx_buf[4] = (uint8_t)msg->misc_1;

    // Pack: Misc. 2
    tx_buf[5] = (uint8_t)msg->misc_2;

    // Pack: Misc. 3
    tx_buf[6] = (uint8_t)msg->misc_3;

    // Pack: Misc. 4
    tx_buf[7] = (uint8_t)msg->misc_4;

    return 0;
}

int unpack_inverter_details(const uint8_t* rx_buf, msg_inverter_details_t* msg) {
    // Unpack: VSM
    msg->vsm = (uint8_t)rx_buf[0];

    // Unpack: PWM Freq
    msg->pwm_freq = (uint8_t)rx_buf[1];

    // Unpack: Inverter
    msg->inverter = (uint8_t)rx_buf[2];

    // Unpack: Relay
    msg->relay = (uint8_t)rx_buf[3];

    // Unpack: Misc. 1
    msg->misc_1 = (uint8_t)rx_buf[4];

    // Unpack: Misc. 2
    msg->misc_2 = (uint8_t)rx_buf[5];

    // Unpack: Misc. 3
    msg->misc_3 = (uint8_t)rx_buf[6];

    // Unpack: Misc. 4
    msg->misc_4 = (uint8_t)rx_buf[7];

    return 0;
}

// Packet: Inverter Faults
int pack_inverter_faults(const msg_inverter_faults_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, INVERTER_FAULTS_DLC);

    // Pack: POST Faults
    tx_buf[0 + 0] = (uint8_t)(msg->post_faults & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((msg->post_faults >> 8) & 0xFF);
    tx_buf[0 + 2] = (uint8_t)((msg->post_faults >> 16) & 0xFF);
    tx_buf[0 + 3] = (uint8_t)((msg->post_faults >> 24) & 0xFF);

    // Pack: Run Faults
    tx_buf[4 + 0] = (uint8_t)(msg->run_faults & 0xFF);
    tx_buf[4 + 1] = (uint8_t)((msg->run_faults >> 8) & 0xFF);
    tx_buf[4 + 2] = (uint8_t)((msg->run_faults >> 16) & 0xFF);
    tx_buf[4 + 3] = (uint8_t)((msg->run_faults >> 24) & 0xFF);

    return 0;
}

int unpack_inverter_faults(const uint8_t* rx_buf, msg_inverter_faults_t* msg) {
    // Unpack: POST Faults
    msg->post_faults = (uint32_t)rx_buf[0 + 0];
    msg->post_faults |= (uint32_t)(rx_buf[0 + 1] << 8);
    msg->post_faults |= (uint32_t)(rx_buf[0 + 2] << 16);
    msg->post_faults |= (uint32_t)(rx_buf[0 + 3] << 24);

    // Unpack: Run Faults
    msg->run_faults = (uint32_t)rx_buf[4 + 0];
    msg->run_faults |= (uint32_t)(rx_buf[4 + 1] << 8);
    msg->run_faults |= (uint32_t)(rx_buf[4 + 2] << 16);
    msg->run_faults |= (uint32_t)(rx_buf[4 + 3] << 24);

    return 0;
}

// Packet: Inverter TSO
int pack_inverter_tso(const msg_inverter_tso_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, INVERTER_TSO_DLC);

    // Pack: commanded torque
    int16_t raw_commanded_torque = (int16_t)(msg->commanded_torque / 0.1f);
    tx_buf[0 + 0] = (uint8_t)(raw_commanded_torque & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_commanded_torque >> 8) & 0xFF);

    // Pack: torque feedback
    int16_t raw_torque_feedback = (int16_t)(msg->torque_feedback / 0.1f);
    tx_buf[2 + 0] = (uint8_t)(raw_torque_feedback & 0xFF);
    tx_buf[2 + 1] = (uint8_t)((raw_torque_feedback >> 8) & 0xFF);

    // Pack: Time since turned ON
    uint32_t raw_time_since_turned_on = (uint32_t)(msg->time_since_turned_on / 0.003f);
    tx_buf[4 + 0] = (uint8_t)(raw_time_since_turned_on & 0xFF);
    tx_buf[4 + 1] = (uint8_t)((raw_time_since_turned_on >> 8) & 0xFF);
    tx_buf[4 + 2] = (uint8_t)((raw_time_since_turned_on >> 16) & 0xFF);
    tx_buf[4 + 3] = (uint8_t)((raw_time_since_turned_on >> 24) & 0xFF);

    return 0;
}

int unpack_inverter_tso(const uint8_t* rx_buf, msg_inverter_tso_t* msg) {
    // Unpack: commanded torque
    int16_t raw_commanded_torque = 0;
    raw_commanded_torque = (int16_t)rx_buf[0 + 0];
    raw_commanded_torque |= (int16_t)(rx_buf[0 + 1] << 8);
    msg->commanded_torque = (float)raw_commanded_torque * 0.1f;

    // Unpack: torque feedback
    int16_t raw_torque_feedback = 0;
    raw_torque_feedback = (int16_t)rx_buf[2 + 0];
    raw_torque_feedback |= (int16_t)(rx_buf[2 + 1] << 8);
    msg->torque_feedback = (float)raw_torque_feedback * 0.1f;

    // Unpack: Time since turned ON
    uint32_t raw_time_since_turned_on = 0;
    raw_time_since_turned_on = (uint32_t)rx_buf[4 + 0];
    raw_time_since_turned_on |= (uint32_t)(rx_buf[4 + 1] << 8);
    raw_time_since_turned_on |= (uint32_t)(rx_buf[4 + 2] << 16);
    raw_time_since_turned_on |= (uint32_t)(rx_buf[4 + 3] << 24);
    msg->time_since_turned_on = (float)raw_time_since_turned_on * 0.003f;

    return 0;
}

// Packet: Inverter Speed
int pack_inverter_speed(const msg_inverter_speed_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, INVERTER_SPEED_DLC);

    // Pack: commanded torque
    int16_t raw_commanded_torque = (int16_t)(msg->commanded_torque / 0.1f);
    tx_buf[0 + 0] = (uint8_t)(raw_commanded_torque & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_commanded_torque >> 8) & 0xFF);

    // Pack: torque feedback
    int16_t raw_torque_feedback = (int16_t)(msg->torque_feedback / 0.1f);
    tx_buf[2 + 0] = (uint8_t)(raw_torque_feedback & 0xFF);
    tx_buf[2 + 1] = (uint8_t)((raw_torque_feedback >> 8) & 0xFF);

    // Pack: Motor Speed
    tx_buf[4 + 0] = (uint8_t)(msg->motor_speed & 0xFF);
    tx_buf[4 + 1] = (uint8_t)((msg->motor_speed >> 8) & 0xFF);

    // Pack: Bus Voltage
    uint16_t raw_bus_voltage = (uint16_t)(msg->bus_voltage / 0.1f);
    tx_buf[6 + 0] = (uint8_t)(raw_bus_voltage & 0xFF);
    tx_buf[6 + 1] = (uint8_t)((raw_bus_voltage >> 8) & 0xFF);

    return 0;
}

int unpack_inverter_speed(const uint8_t* rx_buf, msg_inverter_speed_t* msg) {
    // Unpack: commanded torque
    int16_t raw_commanded_torque = 0;
    raw_commanded_torque = (int16_t)rx_buf[0 + 0];
    raw_commanded_torque |= (int16_t)(rx_buf[0 + 1] << 8);
    msg->commanded_torque = (float)raw_commanded_torque * 0.1f;

    // Unpack: torque feedback
    int16_t raw_torque_feedback = 0;
    raw_torque_feedback = (int16_t)rx_buf[2 + 0];
    raw_torque_feedback |= (int16_t)(rx_buf[2 + 1] << 8);
    msg->torque_feedback = (float)raw_torque_feedback * 0.1f;

    // Unpack: Motor Speed
    msg->motor_speed = (int16_t)rx_buf[4 + 0];
    msg->motor_speed |= (int16_t)(rx_buf[4 + 1] << 8);

    // Unpack: Bus Voltage
    uint16_t raw_bus_voltage = 0;
    raw_bus_voltage = (uint16_t)rx_buf[6 + 0];
    raw_bus_voltage |= (uint16_t)(rx_buf[6 + 1] << 8);
    msg->bus_voltage = (float)raw_bus_voltage * 0.1f;

    return 0;
}

// Packet: Inverter Torque Command
int pack_inverter_torque_command(const msg_inverter_torque_command_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, INVERTER_TORQUE_COMMAND_DLC);

    // Pack: torque request
    int16_t raw_torque_request = (int16_t)(msg->torque_request / 0.1f);
    tx_buf[0 + 0] = (uint8_t)(raw_torque_request & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_torque_request >> 8) & 0xFF);

    // Pack: rpm request
    tx_buf[2 + 0] = (uint8_t)(msg->rpm_request & 0xFF);
    tx_buf[2 + 1] = (uint8_t)((msg->rpm_request >> 8) & 0xFF);

    // Pack: direction
    tx_buf[4] = (uint8_t)msg->direction;

    // Pack: enable
    tx_buf[5] = (uint8_t)msg->enable;

    // Pack: torque limit
    int16_t raw_torque_limit = (int16_t)(msg->torque_limit / 0.1f);
    tx_buf[6 + 0] = (uint8_t)(raw_torque_limit & 0xFF);
    tx_buf[6 + 1] = (uint8_t)((raw_torque_limit >> 8) & 0xFF);

    return 0;
}

int unpack_inverter_torque_command(const uint8_t* rx_buf, msg_inverter_torque_command_t* msg) {
    // Unpack: torque request
    int16_t raw_torque_request = 0;
    raw_torque_request = (int16_t)rx_buf[0 + 0];
    raw_torque_request |= (int16_t)(rx_buf[0 + 1] << 8);
    msg->torque_request = (float)raw_torque_request * 0.1f;

    // Unpack: rpm request
    msg->rpm_request = (int16_t)rx_buf[2 + 0];
    msg->rpm_request |= (int16_t)(rx_buf[2 + 1] << 8);

    // Unpack: direction
    msg->direction = (uint8_t)rx_buf[4];

    // Unpack: enable
    msg->enable = (uint8_t)rx_buf[5];

    // Unpack: torque limit
    int16_t raw_torque_limit = 0;
    raw_torque_limit = (int16_t)rx_buf[6 + 0];
    raw_torque_limit |= (int16_t)(rx_buf[6 + 1] << 8);
    msg->torque_limit = (float)raw_torque_limit * 0.1f;

    return 0;
}

// Packet: Inverter Parameter Request
int pack_inverter_parameter_request(const msg_inverter_parameter_request_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, INVERTER_PARAMETER_REQUEST_DLC);

    // Pack: parameter address
    tx_buf[0 + 0] = (uint8_t)(msg->parameter_address & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((msg->parameter_address >> 8) & 0xFF);

    // Pack: r/w
    tx_buf[2] = (uint8_t)msg->r_w;

    return 0;
}

int unpack_inverter_parameter_request(const uint8_t* rx_buf, msg_inverter_parameter_request_t* msg) {
    // Unpack: parameter address
    msg->parameter_address = (uint16_t)rx_buf[0 + 0];
    msg->parameter_address |= (uint16_t)(rx_buf[0 + 1] << 8);

    // Unpack: r/w
    msg->r_w = (uint8_t)rx_buf[2];

    return 0;
}

// Packet: Inverter Parameter Response
int pack_inverter_parameter_response(const msg_inverter_parameter_response_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, INVERTER_PARAMETER_RESPONSE_DLC);

    // Pack: parameter address
    tx_buf[0 + 0] = (uint8_t)(msg->parameter_address & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((msg->parameter_address >> 8) & 0xFF);

    // Pack: success
    tx_buf[2] = (uint8_t)msg->success;

    return 0;
}

int unpack_inverter_parameter_response(const uint8_t* rx_buf, msg_inverter_parameter_response_t* msg) {
    // Unpack: parameter address
    msg->parameter_address = (uint16_t)rx_buf[0 + 0];
    msg->parameter_address |= (uint16_t)(rx_buf[0 + 1] << 8);

    // Unpack: success
    msg->success = (uint8_t)rx_buf[2];

    return 0;
}

// Packet: Cell Voltages
int pack_cell_voltages(const msg_cell_voltages_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, CELL_VOLTAGES_DLC);

    // Pack: Voltage[i]
    uint16_t raw_voltage_i = (uint16_t)(msg->voltage_i / 0.0001f);
    tx_buf[0 + 0] = (uint8_t)(raw_voltage_i & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_voltage_i >> 8) & 0xFF);

    // Pack: Voltage[i+1]
    uint16_t raw_voltage_i_1 = (uint16_t)(msg->voltage_i_1 / 0.0001f);
    tx_buf[2 + 0] = (uint8_t)(raw_voltage_i_1 & 0xFF);
    tx_buf[2 + 1] = (uint8_t)((raw_voltage_i_1 >> 8) & 0xFF);

    // Pack: Voltage[i+2]
    uint16_t raw_voltage_i_2 = (uint16_t)(msg->voltage_i_2 / 0.0001f);
    tx_buf[4 + 0] = (uint8_t)(raw_voltage_i_2 & 0xFF);
    tx_buf[4 + 1] = (uint8_t)((raw_voltage_i_2 >> 8) & 0xFF);

    // Pack: Voltage[i+3]
    uint16_t raw_voltage_i_3 = (uint16_t)(msg->voltage_i_3 / 0.0001f);
    tx_buf[6 + 0] = (uint8_t)(raw_voltage_i_3 & 0xFF);
    tx_buf[6 + 1] = (uint8_t)((raw_voltage_i_3 >> 8) & 0xFF);

    return 0;
}

int unpack_cell_voltages(const uint8_t* rx_buf, msg_cell_voltages_t* msg) {
    // Unpack: Voltage[i]
    uint16_t raw_voltage_i = 0;
    raw_voltage_i = (uint16_t)rx_buf[0 + 0];
    raw_voltage_i |= (uint16_t)(rx_buf[0 + 1] << 8);
    msg->voltage_i = (float)raw_voltage_i * 0.0001f;

    // Unpack: Voltage[i+1]
    uint16_t raw_voltage_i_1 = 0;
    raw_voltage_i_1 = (uint16_t)rx_buf[2 + 0];
    raw_voltage_i_1 |= (uint16_t)(rx_buf[2 + 1] << 8);
    msg->voltage_i_1 = (float)raw_voltage_i_1 * 0.0001f;

    // Unpack: Voltage[i+2]
    uint16_t raw_voltage_i_2 = 0;
    raw_voltage_i_2 = (uint16_t)rx_buf[4 + 0];
    raw_voltage_i_2 |= (uint16_t)(rx_buf[4 + 1] << 8);
    msg->voltage_i_2 = (float)raw_voltage_i_2 * 0.0001f;

    // Unpack: Voltage[i+3]
    uint16_t raw_voltage_i_3 = 0;
    raw_voltage_i_3 = (uint16_t)rx_buf[6 + 0];
    raw_voltage_i_3 |= (uint16_t)(rx_buf[6 + 1] << 8);
    msg->voltage_i_3 = (float)raw_voltage_i_3 * 0.0001f;

    return 0;
}

// Packet: Cell Temperatures
int pack_cell_temperatures(const msg_cell_temperatures_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, CELL_TEMPERATURES_DLC);

    // Pack: Temp[i]
    uint16_t raw_temp_i = (uint16_t)(msg->temp_i / 0.1f);
    tx_buf[0 + 0] = (uint8_t)(raw_temp_i & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_temp_i >> 8) & 0xFF);

    // Pack: Temp[i+1]
    uint16_t raw_temp_i_1 = (uint16_t)(msg->temp_i_1 / 0.1f);
    tx_buf[2 + 0] = (uint8_t)(raw_temp_i_1 & 0xFF);
    tx_buf[2 + 1] = (uint8_t)((raw_temp_i_1 >> 8) & 0xFF);

    // Pack: Temp[i+2]
    uint16_t raw_temp_i_2 = (uint16_t)(msg->temp_i_2 / 0.1f);
    tx_buf[4 + 0] = (uint8_t)(raw_temp_i_2 & 0xFF);
    tx_buf[4 + 1] = (uint8_t)((raw_temp_i_2 >> 8) & 0xFF);

    // Pack: Temp[i+3]
    uint16_t raw_temp_i_3 = (uint16_t)(msg->temp_i_3 / 0.1f);
    tx_buf[6 + 0] = (uint8_t)(raw_temp_i_3 & 0xFF);
    tx_buf[6 + 1] = (uint8_t)((raw_temp_i_3 >> 8) & 0xFF);

    return 0;
}

int unpack_cell_temperatures(const uint8_t* rx_buf, msg_cell_temperatures_t* msg) {
    // Unpack: Temp[i]
    uint16_t raw_temp_i = 0;
    raw_temp_i = (uint16_t)rx_buf[0 + 0];
    raw_temp_i |= (uint16_t)(rx_buf[0 + 1] << 8);
    msg->temp_i = (float)raw_temp_i * 0.1f;

    // Unpack: Temp[i+1]
    uint16_t raw_temp_i_1 = 0;
    raw_temp_i_1 = (uint16_t)rx_buf[2 + 0];
    raw_temp_i_1 |= (uint16_t)(rx_buf[2 + 1] << 8);
    msg->temp_i_1 = (float)raw_temp_i_1 * 0.1f;

    // Unpack: Temp[i+2]
    uint16_t raw_temp_i_2 = 0;
    raw_temp_i_2 = (uint16_t)rx_buf[4 + 0];
    raw_temp_i_2 |= (uint16_t)(rx_buf[4 + 1] << 8);
    msg->temp_i_2 = (float)raw_temp_i_2 * 0.1f;

    // Unpack: Temp[i+3]
    uint16_t raw_temp_i_3 = 0;
    raw_temp_i_3 = (uint16_t)rx_buf[6 + 0];
    raw_temp_i_3 |= (uint16_t)(rx_buf[6 + 1] << 8);
    msg->temp_i_3 = (float)raw_temp_i_3 * 0.1f;

    return 0;
}

// Packet: DUI R2D Status
int pack_dui_r2d_status(const msg_dui_r2d_status_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, DUI_R2D_STATUS_DLC);

    // Pack: R2D Status
    tx_buf[0] = (uint8_t)msg->r2d_status;

    // Pack: DUI Shutdown Faults
    tx_buf[1] = (uint8_t)msg->dui_shutdown_faults;

    return 0;
}

int unpack_dui_r2d_status(const uint8_t* rx_buf, msg_dui_r2d_status_t* msg) {
    // Unpack: R2D Status
    msg->r2d_status = (uint8_t)rx_buf[0];

    // Unpack: DUI Shutdown Faults
    msg->dui_shutdown_faults = (uint8_t)rx_buf[1];

    return 0;
}

// Packet: DUI R2D Authorization
int pack_dui_r2d_authorization(const msg_dui_r2d_authorization_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, DUI_R2D_AUTHORIZATION_DLC);

    // Pack: R2D Authorized
    tx_buf[0] = (uint8_t)msg->r2d_authorized;

    // Pack: DUI IMD Faults
    tx_buf[1] = (uint8_t)msg->dui_imd_faults;

    // Pack: DUI AMS Faults
    tx_buf[2] = (uint8_t)msg->dui_ams_faults;

    return 0;
}

int unpack_dui_r2d_authorization(const uint8_t* rx_buf, msg_dui_r2d_authorization_t* msg) {
    // Unpack: R2D Authorized
    msg->r2d_authorized = (uint8_t)rx_buf[0];

    // Unpack: DUI IMD Faults
    msg->dui_imd_faults = (uint8_t)rx_buf[1];

    // Unpack: DUI AMS Faults
    msg->dui_ams_faults = (uint8_t)rx_buf[2];

    return 0;
}

// Packet: Wheel Speed, Ride height
int pack_wheel_speed_ride_height(const msg_wheel_speed_ride_height_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, WHEEL_SPEED_RIDE_HEIGHT_DLC);

    // Pack: Wheel Speed
    int16_t raw_wheel_speed = (int16_t)(msg->wheel_speed / 0.0078125f);
    tx_buf[0 + 0] = (uint8_t)(raw_wheel_speed & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_wheel_speed >> 8) & 0xFF);

    // Pack: Ride Height
    uint16_t raw_ride_height = (uint16_t)(msg->ride_height / 0.0625f);
    tx_buf[2 + 0] = (uint8_t)(raw_ride_height & 0xFF);
    tx_buf[2 + 1] = (uint8_t)((raw_ride_height >> 8) & 0xFF);

    return 0;
}

int unpack_wheel_speed_ride_height(const uint8_t* rx_buf, msg_wheel_speed_ride_height_t* msg) {
    // Unpack: Wheel Speed
    int16_t raw_wheel_speed = 0;
    raw_wheel_speed = (int16_t)rx_buf[0 + 0];
    raw_wheel_speed |= (int16_t)(rx_buf[0 + 1] << 8);
    msg->wheel_speed = (float)raw_wheel_speed * 0.0078125f;

    // Unpack: Ride Height
    uint16_t raw_ride_height = 0;
    raw_ride_height = (uint16_t)rx_buf[2 + 0];
    raw_ride_height |= (uint16_t)(rx_buf[2 + 1] << 8);
    msg->ride_height = (float)raw_ride_height * 0.0625f;

    return 0;
}

// Packet: Contactor Status
int pack_contactor_status(const msg_contactor_status_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, CONTACTOR_STATUS_DLC);

    // Pack: HVC State Machine
    tx_buf[0] = (uint8_t)msg->hvc_state_machine;

    // Pack: Positive HV Contactor
    tx_buf[1] = (uint8_t)msg->positive_hv_contactor;

    // Pack: Negative HV Contactor
    tx_buf[2] = (uint8_t)msg->negative_hv_contactor;

    // Pack: Precharge Contactor
    tx_buf[3] = (uint8_t)msg->precharge_contactor;

    return 0;
}

int unpack_contactor_status(const uint8_t* rx_buf, msg_contactor_status_t* msg) {
    // Unpack: HVC State Machine
    msg->hvc_state_machine = (uint8_t)rx_buf[0];

    // Unpack: Positive HV Contactor
    msg->positive_hv_contactor = (uint8_t)rx_buf[1];

    // Unpack: Negative HV Contactor
    msg->negative_hv_contactor = (uint8_t)rx_buf[2];

    // Unpack: Precharge Contactor
    msg->precharge_contactor = (uint8_t)rx_buf[3];

    return 0;
}

// Packet: Battery Pack Status
int pack_battery_pack_status(const msg_battery_pack_status_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, BATTERY_PACK_STATUS_DLC);

    // Pack: Pack Voltage
    uint16_t raw_pack_voltage = (uint16_t)(msg->pack_voltage / 0.01f);
    tx_buf[0 + 0] = (uint8_t)(raw_pack_voltage & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_pack_voltage >> 8) & 0xFF);

    // Pack: Tractive Current
    uint16_t raw_tractive_current = (uint16_t)(msg->tractive_current / 0.01f);
    tx_buf[2 + 0] = (uint8_t)(raw_tractive_current & 0xFF);
    tx_buf[2 + 1] = (uint8_t)((raw_tractive_current >> 8) & 0xFF);

    // Pack: State of Charge
    uint16_t raw_state_of_charge = (uint16_t)(msg->state_of_charge / 0.01f);
    tx_buf[4 + 0] = (uint8_t)(raw_state_of_charge & 0xFF);
    tx_buf[4 + 1] = (uint8_t)((raw_state_of_charge >> 8) & 0xFF);

    // Pack: Cell Top Temp
    tx_buf[6] = (uint8_t)msg->cell_top_temp;

    // Pack: Cell Bottom Temp
    tx_buf[7] = (uint8_t)msg->cell_bottom_temp;

    return 0;
}

int unpack_battery_pack_status(const uint8_t* rx_buf, msg_battery_pack_status_t* msg) {
    // Unpack: Pack Voltage
    uint16_t raw_pack_voltage = 0;
    raw_pack_voltage = (uint16_t)rx_buf[0 + 0];
    raw_pack_voltage |= (uint16_t)(rx_buf[0 + 1] << 8);
    msg->pack_voltage = (float)raw_pack_voltage * 0.01f;

    // Unpack: Tractive Current
    uint16_t raw_tractive_current = 0;
    raw_tractive_current = (uint16_t)rx_buf[2 + 0];
    raw_tractive_current |= (uint16_t)(rx_buf[2 + 1] << 8);
    msg->tractive_current = (float)raw_tractive_current * 0.01f;

    // Unpack: State of Charge
    uint16_t raw_state_of_charge = 0;
    raw_state_of_charge = (uint16_t)rx_buf[4 + 0];
    raw_state_of_charge |= (uint16_t)(rx_buf[4 + 1] << 8);
    msg->state_of_charge = (float)raw_state_of_charge * 0.01f;

    // Unpack: Cell Top Temp
    msg->cell_top_temp = (uint8_t)rx_buf[6];

    // Unpack: Cell Bottom Temp
    msg->cell_bottom_temp = (uint8_t)rx_buf[7];

    return 0;
}

// Packet: Battery Temperature Status
int pack_battery_temperature_status(const msg_battery_temperature_status_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, BATTERY_TEMPERATURE_STATUS_DLC);

    // Pack: Bus Bar 1 Temp
    uint16_t raw_bus_bar_1_temp = (uint16_t)(msg->bus_bar_1_temp / 0.1f);
    tx_buf[0 + 0] = (uint8_t)(raw_bus_bar_1_temp & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_bus_bar_1_temp >> 8) & 0xFF);

    // Pack: Bus Bar 2 Temp
    uint16_t raw_bus_bar_2_temp = (uint16_t)(msg->bus_bar_2_temp / 0.1f);
    tx_buf[2 + 0] = (uint8_t)(raw_bus_bar_2_temp & 0xFF);
    tx_buf[2 + 1] = (uint8_t)((raw_bus_bar_2_temp >> 8) & 0xFF);

    // Pack: Bus Bar 3 Temp
    uint16_t raw_bus_bar_3_temp = (uint16_t)(msg->bus_bar_3_temp / 0.1f);
    tx_buf[4 + 0] = (uint8_t)(raw_bus_bar_3_temp & 0xFF);
    tx_buf[4 + 1] = (uint8_t)((raw_bus_bar_3_temp >> 8) & 0xFF);

    // Pack: Precharge Resistor Temp
    uint16_t raw_precharge_resistor_temp = (uint16_t)(msg->precharge_resistor_temp / 0.1f);
    tx_buf[6 + 0] = (uint8_t)(raw_precharge_resistor_temp & 0xFF);
    tx_buf[6 + 1] = (uint8_t)((raw_precharge_resistor_temp >> 8) & 0xFF);

    return 0;
}

int unpack_battery_temperature_status(const uint8_t* rx_buf, msg_battery_temperature_status_t* msg) {
    // Unpack: Bus Bar 1 Temp
    uint16_t raw_bus_bar_1_temp = 0;
    raw_bus_bar_1_temp = (uint16_t)rx_buf[0 + 0];
    raw_bus_bar_1_temp |= (uint16_t)(rx_buf[0 + 1] << 8);
    msg->bus_bar_1_temp = (float)raw_bus_bar_1_temp * 0.1f;

    // Unpack: Bus Bar 2 Temp
    uint16_t raw_bus_bar_2_temp = 0;
    raw_bus_bar_2_temp = (uint16_t)rx_buf[2 + 0];
    raw_bus_bar_2_temp |= (uint16_t)(rx_buf[2 + 1] << 8);
    msg->bus_bar_2_temp = (float)raw_bus_bar_2_temp * 0.1f;

    // Unpack: Bus Bar 3 Temp
    uint16_t raw_bus_bar_3_temp = 0;
    raw_bus_bar_3_temp = (uint16_t)rx_buf[4 + 0];
    raw_bus_bar_3_temp |= (uint16_t)(rx_buf[4 + 1] << 8);
    msg->bus_bar_3_temp = (float)raw_bus_bar_3_temp * 0.1f;

    // Unpack: Precharge Resistor Temp
    uint16_t raw_precharge_resistor_temp = 0;
    raw_precharge_resistor_temp = (uint16_t)rx_buf[6 + 0];
    raw_precharge_resistor_temp |= (uint16_t)(rx_buf[6 + 1] << 8);
    msg->precharge_resistor_temp = (float)raw_precharge_resistor_temp * 0.1f;

    return 0;
}

// Packet: Indicators + Shutdown Status
int pack_indicators_shutdown_status(const msg_indicators_shutdown_status_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, INDICATORS_SHUTDOWN_STATUS_DLC);

    // Pack: BMS Error
    tx_buf[0] = (uint8_t)msg->bms_error;

    // Pack: IMD Error
    tx_buf[1] = (uint8_t)msg->imd_error;

    // Pack: Shutdown Leg 1
    tx_buf[2] = (uint8_t)msg->shutdown_leg_1;

    // Pack: Shutdown Leg 2
    tx_buf[3] = (uint8_t)msg->shutdown_leg_2;

    // Pack: Shutdown Leg 3
    tx_buf[4] = (uint8_t)msg->shutdown_leg_3;

    // Pack: Shutdown Leg 4
    tx_buf[5] = (uint8_t)msg->shutdown_leg_4;

    return 0;
}

int unpack_indicators_shutdown_status(const uint8_t* rx_buf, msg_indicators_shutdown_status_t* msg) {
    // Unpack: BMS Error
    msg->bms_error = (uint8_t)rx_buf[0];

    // Unpack: IMD Error
    msg->imd_error = (uint8_t)rx_buf[1];

    // Unpack: Shutdown Leg 1
    msg->shutdown_leg_1 = (uint8_t)rx_buf[2];

    // Unpack: Shutdown Leg 2
    msg->shutdown_leg_2 = (uint8_t)rx_buf[3];

    // Unpack: Shutdown Leg 3
    msg->shutdown_leg_3 = (uint8_t)rx_buf[4];

    // Unpack: Shutdown Leg 4
    msg->shutdown_leg_4 = (uint8_t)rx_buf[5];

    return 0;
}

// Packet: Allow Balance Command
int pack_allow_balance_command(const msg_allow_balance_command_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, ALLOW_BALANCE_COMMAND_DLC);

    return 0;
}

int unpack_allow_balance_command(const uint8_t* rx_buf, msg_allow_balance_command_t* msg) {
    return 0;
}

// Packet: Battery Cell Limits
int pack_battery_cell_limits(const msg_battery_cell_limits_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, BATTERY_CELL_LIMITS_DLC);

    // Pack: Min Cell Voltage
    uint16_t raw_min_cell_voltage = (uint16_t)(msg->min_cell_voltage / 0.0001f);
    tx_buf[0 + 0] = (uint8_t)(raw_min_cell_voltage & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_min_cell_voltage >> 8) & 0xFF);

    return 0;
}

int unpack_battery_cell_limits(const uint8_t* rx_buf, msg_battery_cell_limits_t* msg) {
    // Unpack: Min Cell Voltage
    uint16_t raw_min_cell_voltage = 0;
    raw_min_cell_voltage = (uint16_t)rx_buf[0 + 0];
    raw_min_cell_voltage |= (uint16_t)(rx_buf[0 + 1] << 8);
    msg->min_cell_voltage = (float)raw_min_cell_voltage * 0.0001f;

    return 0;
}

// Packet: VCU Shutdown Status
int pack_vcu_shutdown_status(const msg_vcu_shutdown_status_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, VCU_SHUTDOWN_STATUS_DLC);

    // Pack: VCU Shutdown Status
    tx_buf[0] = (uint8_t)msg->vcu_shutdown_status;

    return 0;
}

int unpack_vcu_shutdown_status(const uint8_t* rx_buf, msg_vcu_shutdown_status_t* msg) {
    // Unpack: VCU Shutdown Status
    msg->vcu_shutdown_status = (uint8_t)rx_buf[0];

    return 0;
}

// Packet: VCU Fuses
int pack_vcu_fuses(const msg_vcu_fuses_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, VCU_FUSES_DLC);

    // Pack: VCU Fuses 1
    tx_buf[0] = (uint8_t)msg->vcu_fuses_1;

    // Pack: VCU Fuses 2
    tx_buf[1] = (uint8_t)msg->vcu_fuses_2;

    return 0;
}

int unpack_vcu_fuses(const uint8_t* rx_buf, msg_vcu_fuses_t* msg) {
    // Unpack: VCU Fuses 1
    msg->vcu_fuses_1 = (uint8_t)rx_buf[0];

    // Unpack: VCU Fuses 2
    msg->vcu_fuses_2 = (uint8_t)rx_buf[1];

    return 0;
}

// Packet: VCU Current Sense
int pack_vcu_current_sense(const msg_vcu_current_sense_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, VCU_CURRENT_SENSE_DLC);

    // Pack: LV Boards Current
    uint8_t raw_lv_boards_current = (uint8_t)(msg->lv_boards_current / 0.04f);
    tx_buf[0] = (uint8_t)raw_lv_boards_current;

    // Pack: Shutdown Current
    uint8_t raw_shutdown_current = (uint8_t)(msg->shutdown_current / 0.04f);
    tx_buf[1] = (uint8_t)raw_shutdown_current;

    // Pack: Battery Cooling Current
    uint8_t raw_battery_cooling_current = (uint8_t)(msg->battery_cooling_current / 0.04f);
    tx_buf[2] = (uint8_t)raw_battery_cooling_current;

    // Pack: Motor Cooling Current
    uint8_t raw_motor_cooling_current = (uint8_t)(msg->motor_cooling_current / 0.04f);
    tx_buf[3] = (uint8_t)raw_motor_cooling_current;

    // Pack: Lights Current Current
    uint8_t raw_lights_current_current = (uint8_t)(msg->lights_current_current / 0.04f);
    tx_buf[4] = (uint8_t)raw_lights_current_current;

    return 0;
}

int unpack_vcu_current_sense(const uint8_t* rx_buf, msg_vcu_current_sense_t* msg) {
    // Unpack: LV Boards Current
    uint8_t raw_lv_boards_current = 0;
    raw_lv_boards_current = (uint8_t)rx_buf[0];
    msg->lv_boards_current = (float)raw_lv_boards_current * 0.04f;

    // Unpack: Shutdown Current
    uint8_t raw_shutdown_current = 0;
    raw_shutdown_current = (uint8_t)rx_buf[1];
    msg->shutdown_current = (float)raw_shutdown_current * 0.04f;

    // Unpack: Battery Cooling Current
    uint8_t raw_battery_cooling_current = 0;
    raw_battery_cooling_current = (uint8_t)rx_buf[2];
    msg->battery_cooling_current = (float)raw_battery_cooling_current * 0.04f;

    // Unpack: Motor Cooling Current
    uint8_t raw_motor_cooling_current = 0;
    raw_motor_cooling_current = (uint8_t)rx_buf[3];
    msg->motor_cooling_current = (float)raw_motor_cooling_current * 0.04f;

    // Unpack: Lights Current Current
    uint8_t raw_lights_current_current = 0;
    raw_lights_current_current = (uint8_t)rx_buf[4];
    msg->lights_current_current = (float)raw_lights_current_current * 0.04f;

    return 0;
}

// Packet: Switch Command
int pack_switch_command(const msg_switch_command_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, SWITCH_COMMAND_DLC);

    // Pack: Switch Command
    tx_buf[0] = (uint8_t)msg->switch_command;

    return 0;
}

int unpack_switch_command(const uint8_t* rx_buf, msg_switch_command_t* msg) {
    // Unpack: Switch Command
    msg->switch_command = (uint8_t)rx_buf[0];

    return 0;
}

// Packet: Switch Outputs
int pack_switch_outputs(const msg_switch_outputs_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, SWITCH_OUTPUTS_DLC);

    // Pack: Switch Output
    tx_buf[0] = (uint8_t)msg->switch_output;

    return 0;
}

int unpack_switch_outputs(const uint8_t* rx_buf, msg_switch_outputs_t* msg) {
    // Unpack: Switch Output
    msg->switch_output = (uint8_t)rx_buf[0];

    return 0;
}

// Packet: Battery Cooling
int pack_battery_cooling(const msg_battery_cooling_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, BATTERY_COOLING_DLC);

    // Pack: Temp After Battery
    int16_t raw_temp_after_battery = (int16_t)(msg->temp_after_battery / 0.01f);
    tx_buf[0 + 0] = (uint8_t)(raw_temp_after_battery & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_temp_after_battery >> 8) & 0xFF);

    // Pack: Temp After Radiator
    int16_t raw_temp_after_radiator = (int16_t)(msg->temp_after_radiator / 0.01f);
    tx_buf[2 + 0] = (uint8_t)(raw_temp_after_radiator & 0xFF);
    tx_buf[2 + 1] = (uint8_t)((raw_temp_after_radiator >> 8) & 0xFF);

    // Pack: Radiator Fan Speed
    uint16_t raw_radiator_fan_speed = (uint16_t)(msg->radiator_fan_speed / 0.2f);
    tx_buf[4 + 0] = (uint8_t)(raw_radiator_fan_speed & 0xFF);
    tx_buf[4 + 1] = (uint8_t)((raw_radiator_fan_speed >> 8) & 0xFF);

    return 0;
}

int unpack_battery_cooling(const uint8_t* rx_buf, msg_battery_cooling_t* msg) {
    // Unpack: Temp After Battery
    int16_t raw_temp_after_battery = 0;
    raw_temp_after_battery = (int16_t)rx_buf[0 + 0];
    raw_temp_after_battery |= (int16_t)(rx_buf[0 + 1] << 8);
    msg->temp_after_battery = (float)raw_temp_after_battery * 0.01f;

    // Unpack: Temp After Radiator
    int16_t raw_temp_after_radiator = 0;
    raw_temp_after_radiator = (int16_t)rx_buf[2 + 0];
    raw_temp_after_radiator |= (int16_t)(rx_buf[2 + 1] << 8);
    msg->temp_after_radiator = (float)raw_temp_after_radiator * 0.01f;

    // Unpack: Radiator Fan Speed
    uint16_t raw_radiator_fan_speed = 0;
    raw_radiator_fan_speed = (uint16_t)rx_buf[4 + 0];
    raw_radiator_fan_speed |= (uint16_t)(rx_buf[4 + 1] << 8);
    msg->radiator_fan_speed = (float)raw_radiator_fan_speed * 0.2f;

    return 0;
}

// Packet: Temps
int pack_temps(const msg_temps_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, TEMPS_DLC);

    // Pack: Inverter
    int16_t raw_inverter = (int16_t)(msg->inverter / 0.01f);
    tx_buf[0 + 0] = (uint8_t)(raw_inverter & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_inverter >> 8) & 0xFF);

    // Pack: Motor
    int16_t raw_motor = (int16_t)(msg->motor / 0.01f);
    tx_buf[2 + 0] = (uint8_t)(raw_motor & 0xFF);
    tx_buf[2 + 1] = (uint8_t)((raw_motor >> 8) & 0xFF);

    // Pack: Ambient
    int16_t raw_ambient = (int16_t)(msg->ambient / 0.01f);
    tx_buf[4 + 0] = (uint8_t)(raw_ambient & 0xFF);
    tx_buf[4 + 1] = (uint8_t)((raw_ambient >> 8) & 0xFF);

    // Pack: Discharge Resistor Temp
    int16_t raw_discharge_resistor_temp = (int16_t)(msg->discharge_resistor_temp / 0.01f);
    tx_buf[6 + 0] = (uint8_t)(raw_discharge_resistor_temp & 0xFF);
    tx_buf[6 + 1] = (uint8_t)((raw_discharge_resistor_temp >> 8) & 0xFF);

    return 0;
}

int unpack_temps(const uint8_t* rx_buf, msg_temps_t* msg) {
    // Unpack: Inverter
    int16_t raw_inverter = 0;
    raw_inverter = (int16_t)rx_buf[0 + 0];
    raw_inverter |= (int16_t)(rx_buf[0 + 1] << 8);
    msg->inverter = (float)raw_inverter * 0.01f;

    // Unpack: Motor
    int16_t raw_motor = 0;
    raw_motor = (int16_t)rx_buf[2 + 0];
    raw_motor |= (int16_t)(rx_buf[2 + 1] << 8);
    msg->motor = (float)raw_motor * 0.01f;

    // Unpack: Ambient
    int16_t raw_ambient = 0;
    raw_ambient = (int16_t)rx_buf[4 + 0];
    raw_ambient |= (int16_t)(rx_buf[4 + 1] << 8);
    msg->ambient = (float)raw_ambient * 0.01f;

    // Unpack: Discharge Resistor Temp
    int16_t raw_discharge_resistor_temp = 0;
    raw_discharge_resistor_temp = (int16_t)rx_buf[6 + 0];
    raw_discharge_resistor_temp |= (int16_t)(rx_buf[6 + 1] << 8);
    msg->discharge_resistor_temp = (float)raw_discharge_resistor_temp * 0.01f;

    return 0;
}

// Packet: LV Battery
int pack_lv_battery(const msg_lv_battery_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, LV_BATTERY_DLC);

    // Pack: LV Battery Voltage
    uint16_t raw_lv_battery_voltage = (uint16_t)(msg->lv_battery_voltage / 0.01f);
    tx_buf[0 + 0] = (uint8_t)(raw_lv_battery_voltage & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_lv_battery_voltage >> 8) & 0xFF);

    // Pack: LV Battery Current
    int16_t raw_lv_battery_current = (int16_t)(msg->lv_battery_current / 0.01f);
    tx_buf[2 + 0] = (uint8_t)(raw_lv_battery_current & 0xFF);
    tx_buf[2 + 1] = (uint8_t)((raw_lv_battery_current >> 8) & 0xFF);

    // Pack: LV Battery Temp
    int16_t raw_lv_battery_temp = (int16_t)(msg->lv_battery_temp / 0.01f);
    tx_buf[4 + 0] = (uint8_t)(raw_lv_battery_temp & 0xFF);
    tx_buf[4 + 1] = (uint8_t)((raw_lv_battery_temp >> 8) & 0xFF);

    return 0;
}

int unpack_lv_battery(const uint8_t* rx_buf, msg_lv_battery_t* msg) {
    // Unpack: LV Battery Voltage
    uint16_t raw_lv_battery_voltage = 0;
    raw_lv_battery_voltage = (uint16_t)rx_buf[0 + 0];
    raw_lv_battery_voltage |= (uint16_t)(rx_buf[0 + 1] << 8);
    msg->lv_battery_voltage = (float)raw_lv_battery_voltage * 0.01f;

    // Unpack: LV Battery Current
    int16_t raw_lv_battery_current = 0;
    raw_lv_battery_current = (int16_t)rx_buf[2 + 0];
    raw_lv_battery_current |= (int16_t)(rx_buf[2 + 1] << 8);
    msg->lv_battery_current = (float)raw_lv_battery_current * 0.01f;

    // Unpack: LV Battery Temp
    int16_t raw_lv_battery_temp = 0;
    raw_lv_battery_temp = (int16_t)rx_buf[4 + 0];
    raw_lv_battery_temp |= (int16_t)(rx_buf[4 + 1] << 8);
    msg->lv_battery_temp = (float)raw_lv_battery_temp * 0.01f;

    return 0;
}

// Packet: Coolant Loop Temps
int pack_coolant_loop_temps(const msg_coolant_loop_temps_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, COOLANT_LOOP_TEMPS_DLC);

    // Pack: Loop Temp After Motor
    int16_t raw_loop_temp_after_motor = (int16_t)(msg->loop_temp_after_motor / 0.01f);
    tx_buf[0 + 0] = (uint8_t)(raw_loop_temp_after_motor & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_loop_temp_after_motor >> 8) & 0xFF);

    // Pack: Loop Temp After Inverter
    int16_t raw_loop_temp_after_inverter = (int16_t)(msg->loop_temp_after_inverter / 0.01f);
    tx_buf[2 + 0] = (uint8_t)(raw_loop_temp_after_inverter & 0xFF);
    tx_buf[2 + 1] = (uint8_t)((raw_loop_temp_after_inverter >> 8) & 0xFF);

    // Pack: Temp After Radiator
    int16_t raw_temp_after_radiator = (int16_t)(msg->temp_after_radiator / 0.01f);
    tx_buf[4 + 0] = (uint8_t)(raw_temp_after_radiator & 0xFF);
    tx_buf[4 + 1] = (uint8_t)((raw_temp_after_radiator >> 8) & 0xFF);

    return 0;
}

int unpack_coolant_loop_temps(const uint8_t* rx_buf, msg_coolant_loop_temps_t* msg) {
    // Unpack: Loop Temp After Motor
    int16_t raw_loop_temp_after_motor = 0;
    raw_loop_temp_after_motor = (int16_t)rx_buf[0 + 0];
    raw_loop_temp_after_motor |= (int16_t)(rx_buf[0 + 1] << 8);
    msg->loop_temp_after_motor = (float)raw_loop_temp_after_motor * 0.01f;

    // Unpack: Loop Temp After Inverter
    int16_t raw_loop_temp_after_inverter = 0;
    raw_loop_temp_after_inverter = (int16_t)rx_buf[2 + 0];
    raw_loop_temp_after_inverter |= (int16_t)(rx_buf[2 + 1] << 8);
    msg->loop_temp_after_inverter = (float)raw_loop_temp_after_inverter * 0.01f;

    // Unpack: Temp After Radiator
    int16_t raw_temp_after_radiator = 0;
    raw_temp_after_radiator = (int16_t)rx_buf[4 + 0];
    raw_temp_after_radiator |= (int16_t)(rx_buf[4 + 1] << 8);
    msg->temp_after_radiator = (float)raw_temp_after_radiator * 0.01f;

    return 0;
}

// Packet: Fan/Flow Speeds
int pack_fan_flow_speeds(const msg_fan_flow_speeds_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, FAN_FLOW_SPEEDS_DLC);

    // Pack: Radiator Fan Speed
    tx_buf[0 + 0] = (uint8_t)(msg->radiator_fan_speed & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((msg->radiator_fan_speed >> 8) & 0xFF);

    // Pack: Battery Fan Speed
    tx_buf[2 + 0] = (uint8_t)(msg->battery_fan_speed & 0xFF);
    tx_buf[2 + 1] = (uint8_t)((msg->battery_fan_speed >> 8) & 0xFF);

    // Pack: Coolant Flow
    uint16_t raw_coolant_flow = (uint16_t)(msg->coolant_flow / 0.01f);
    tx_buf[4 + 0] = (uint8_t)(raw_coolant_flow & 0xFF);
    tx_buf[4 + 1] = (uint8_t)((raw_coolant_flow >> 8) & 0xFF);

    // Pack: Ambient Temp
    int16_t raw_ambient_temp = (int16_t)(msg->ambient_temp / 0.01f);
    tx_buf[6 + 0] = (uint8_t)(raw_ambient_temp & 0xFF);
    tx_buf[6 + 1] = (uint8_t)((raw_ambient_temp >> 8) & 0xFF);

    return 0;
}

int unpack_fan_flow_speeds(const uint8_t* rx_buf, msg_fan_flow_speeds_t* msg) {
    // Unpack: Radiator Fan Speed
    msg->radiator_fan_speed = (uint16_t)rx_buf[0 + 0];
    msg->radiator_fan_speed |= (uint16_t)(rx_buf[0 + 1] << 8);

    // Unpack: Battery Fan Speed
    msg->battery_fan_speed = (uint16_t)rx_buf[2 + 0];
    msg->battery_fan_speed |= (uint16_t)(rx_buf[2 + 1] << 8);

    // Unpack: Coolant Flow
    uint16_t raw_coolant_flow = 0;
    raw_coolant_flow = (uint16_t)rx_buf[4 + 0];
    raw_coolant_flow |= (uint16_t)(rx_buf[4 + 1] << 8);
    msg->coolant_flow = (float)raw_coolant_flow * 0.01f;

    // Unpack: Ambient Temp
    int16_t raw_ambient_temp = 0;
    raw_ambient_temp = (int16_t)rx_buf[6 + 0];
    raw_ambient_temp |= (int16_t)(rx_buf[6 + 1] << 8);
    msg->ambient_temp = (float)raw_ambient_temp * 0.01f;

    return 0;
}

// Packet: APPS Voltages
int pack_apps_voltages(const msg_apps_voltages_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, APPS_VOLTAGES_DLC);

    // Pack: APPS1 Voltage
    uint16_t raw_apps1_voltage = (uint16_t)(msg->apps1_voltage / 0.0001f);
    tx_buf[0 + 0] = (uint8_t)(raw_apps1_voltage & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_apps1_voltage >> 8) & 0xFF);

    // Pack: APPS2 Voltage
    uint16_t raw_apps2_voltage = (uint16_t)(msg->apps2_voltage / 0.0001f);
    tx_buf[2 + 0] = (uint8_t)(raw_apps2_voltage & 0xFF);
    tx_buf[2 + 1] = (uint8_t)((raw_apps2_voltage >> 8) & 0xFF);

    // Pack: APPS1 Travel
    uint16_t raw_apps1_travel = (uint16_t)(msg->apps1_travel / 0.0001f);
    tx_buf[4 + 0] = (uint8_t)(raw_apps1_travel & 0xFF);
    tx_buf[4 + 1] = (uint8_t)((raw_apps1_travel >> 8) & 0xFF);

    // Pack: APPS2 Travel
    uint16_t raw_apps2_travel = (uint16_t)(msg->apps2_travel / 0.0001f);
    tx_buf[6 + 0] = (uint8_t)(raw_apps2_travel & 0xFF);
    tx_buf[6 + 1] = (uint8_t)((raw_apps2_travel >> 8) & 0xFF);

    return 0;
}

int unpack_apps_voltages(const uint8_t* rx_buf, msg_apps_voltages_t* msg) {
    // Unpack: APPS1 Voltage
    uint16_t raw_apps1_voltage = 0;
    raw_apps1_voltage = (uint16_t)rx_buf[0 + 0];
    raw_apps1_voltage |= (uint16_t)(rx_buf[0 + 1] << 8);
    msg->apps1_voltage = (float)raw_apps1_voltage * 0.0001f;

    // Unpack: APPS2 Voltage
    uint16_t raw_apps2_voltage = 0;
    raw_apps2_voltage = (uint16_t)rx_buf[2 + 0];
    raw_apps2_voltage |= (uint16_t)(rx_buf[2 + 1] << 8);
    msg->apps2_voltage = (float)raw_apps2_voltage * 0.0001f;

    // Unpack: APPS1 Travel
    uint16_t raw_apps1_travel = 0;
    raw_apps1_travel = (uint16_t)rx_buf[4 + 0];
    raw_apps1_travel |= (uint16_t)(rx_buf[4 + 1] << 8);
    msg->apps1_travel = (float)raw_apps1_travel * 0.0001f;

    // Unpack: APPS2 Travel
    uint16_t raw_apps2_travel = 0;
    raw_apps2_travel = (uint16_t)rx_buf[6 + 0];
    raw_apps2_travel |= (uint16_t)(rx_buf[6 + 1] << 8);
    msg->apps2_travel = (float)raw_apps2_travel * 0.0001f;

    return 0;
}

// Packet: Accelerator Pedal
int pack_accelerator_pedal(const msg_accelerator_pedal_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, ACCELERATOR_PEDAL_DLC);

    // Pack: Accelerator Pedal Travel
    uint16_t raw_accelerator_pedal_travel = (uint16_t)(msg->accelerator_pedal_travel / 0.0001f);
    tx_buf[0 + 0] = (uint8_t)(raw_accelerator_pedal_travel & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_accelerator_pedal_travel >> 8) & 0xFF);

    // Pack: APPS Faults
    tx_buf[2] = (uint8_t)msg->apps_faults;

    return 0;
}

int unpack_accelerator_pedal(const uint8_t* rx_buf, msg_accelerator_pedal_t* msg) {
    // Unpack: Accelerator Pedal Travel
    uint16_t raw_accelerator_pedal_travel = 0;
    raw_accelerator_pedal_travel = (uint16_t)rx_buf[0 + 0];
    raw_accelerator_pedal_travel |= (uint16_t)(rx_buf[0 + 1] << 8);
    msg->accelerator_pedal_travel = (float)raw_accelerator_pedal_travel * 0.0001f;

    // Unpack: APPS Faults
    msg->apps_faults = (uint8_t)rx_buf[2];

    return 0;
}

// Packet: BPPS Voltages
int pack_bpps_voltages(const msg_bpps_voltages_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, BPPS_VOLTAGES_DLC);

    // Pack: BPPS1 Voltage
    uint16_t raw_bpps1_voltage = (uint16_t)(msg->bpps1_voltage / 0.0001f);
    tx_buf[0 + 0] = (uint8_t)(raw_bpps1_voltage & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_bpps1_voltage >> 8) & 0xFF);

    // Pack: BPPS2 Voltage
    uint16_t raw_bpps2_voltage = (uint16_t)(msg->bpps2_voltage / 0.0001f);
    tx_buf[2 + 0] = (uint8_t)(raw_bpps2_voltage & 0xFF);
    tx_buf[2 + 1] = (uint8_t)((raw_bpps2_voltage >> 8) & 0xFF);

    // Pack: BPPS1 Travel
    uint16_t raw_bpps1_travel = (uint16_t)(msg->bpps1_travel / 0.0001f);
    tx_buf[4 + 0] = (uint8_t)(raw_bpps1_travel & 0xFF);
    tx_buf[4 + 1] = (uint8_t)((raw_bpps1_travel >> 8) & 0xFF);

    // Pack: BPPS2 Travel
    uint16_t raw_bpps2_travel = (uint16_t)(msg->bpps2_travel / 0.0001f);
    tx_buf[6 + 0] = (uint8_t)(raw_bpps2_travel & 0xFF);
    tx_buf[6 + 1] = (uint8_t)((raw_bpps2_travel >> 8) & 0xFF);

    return 0;
}

int unpack_bpps_voltages(const uint8_t* rx_buf, msg_bpps_voltages_t* msg) {
    // Unpack: BPPS1 Voltage
    uint16_t raw_bpps1_voltage = 0;
    raw_bpps1_voltage = (uint16_t)rx_buf[0 + 0];
    raw_bpps1_voltage |= (uint16_t)(rx_buf[0 + 1] << 8);
    msg->bpps1_voltage = (float)raw_bpps1_voltage * 0.0001f;

    // Unpack: BPPS2 Voltage
    uint16_t raw_bpps2_voltage = 0;
    raw_bpps2_voltage = (uint16_t)rx_buf[2 + 0];
    raw_bpps2_voltage |= (uint16_t)(rx_buf[2 + 1] << 8);
    msg->bpps2_voltage = (float)raw_bpps2_voltage * 0.0001f;

    // Unpack: BPPS1 Travel
    uint16_t raw_bpps1_travel = 0;
    raw_bpps1_travel = (uint16_t)rx_buf[4 + 0];
    raw_bpps1_travel |= (uint16_t)(rx_buf[4 + 1] << 8);
    msg->bpps1_travel = (float)raw_bpps1_travel * 0.0001f;

    // Unpack: BPPS2 Travel
    uint16_t raw_bpps2_travel = 0;
    raw_bpps2_travel = (uint16_t)rx_buf[6 + 0];
    raw_bpps2_travel |= (uint16_t)(rx_buf[6 + 1] << 8);
    msg->bpps2_travel = (float)raw_bpps2_travel * 0.0001f;

    return 0;
}

// Packet: Brake Pedal
int pack_brake_pedal(const msg_brake_pedal_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, BRAKE_PEDAL_DLC);

    // Pack: Brake Pedal Travel
    uint16_t raw_brake_pedal_travel = (uint16_t)(msg->brake_pedal_travel / 0.0001f);
    tx_buf[0 + 0] = (uint8_t)(raw_brake_pedal_travel & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_brake_pedal_travel >> 8) & 0xFF);

    // Pack: BPPS Faults
    tx_buf[2] = (uint8_t)msg->bpps_faults;

    // Pack: Brake Light Percent
    uint16_t raw_brake_light_percent = (uint16_t)(msg->brake_light_percent / 0.001f);
    tx_buf[3 + 0] = (uint8_t)(raw_brake_light_percent & 0xFF);
    tx_buf[3 + 1] = (uint8_t)((raw_brake_light_percent >> 8) & 0xFF);

    return 0;
}

int unpack_brake_pedal(const uint8_t* rx_buf, msg_brake_pedal_t* msg) {
    // Unpack: Brake Pedal Travel
    uint16_t raw_brake_pedal_travel = 0;
    raw_brake_pedal_travel = (uint16_t)rx_buf[0 + 0];
    raw_brake_pedal_travel |= (uint16_t)(rx_buf[0 + 1] << 8);
    msg->brake_pedal_travel = (float)raw_brake_pedal_travel * 0.0001f;

    // Unpack: BPPS Faults
    msg->bpps_faults = (uint8_t)rx_buf[2];

    // Unpack: Brake Light Percent
    uint16_t raw_brake_light_percent = 0;
    raw_brake_light_percent = (uint16_t)rx_buf[3 + 0];
    raw_brake_light_percent |= (uint16_t)(rx_buf[3 + 1] << 8);
    msg->brake_light_percent = (float)raw_brake_light_percent * 0.001f;

    return 0;
}

// Packet: BSE Voltages
int pack_bse_voltages(const msg_bse_voltages_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, BSE_VOLTAGES_DLC);

    // Pack: BSE Front Voltage
    uint16_t raw_bse_front_voltage = (uint16_t)(msg->bse_front_voltage / 0.0001f);
    tx_buf[0 + 0] = (uint8_t)(raw_bse_front_voltage & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_bse_front_voltage >> 8) & 0xFF);

    // Pack: BSE Rear Voltage
    uint16_t raw_bse_rear_voltage = (uint16_t)(msg->bse_rear_voltage / 0.0001f);
    tx_buf[2 + 0] = (uint8_t)(raw_bse_rear_voltage & 0xFF);
    tx_buf[2 + 1] = (uint8_t)((raw_bse_rear_voltage >> 8) & 0xFF);

    // Pack: BSE Line Lock Voltage
    uint16_t raw_bse_line_lock_voltage = (uint16_t)(msg->bse_line_lock_voltage / 0.0001f);
    tx_buf[4 + 0] = (uint8_t)(raw_bse_line_lock_voltage & 0xFF);
    tx_buf[4 + 1] = (uint8_t)((raw_bse_line_lock_voltage >> 8) & 0xFF);

    return 0;
}

int unpack_bse_voltages(const uint8_t* rx_buf, msg_bse_voltages_t* msg) {
    // Unpack: BSE Front Voltage
    uint16_t raw_bse_front_voltage = 0;
    raw_bse_front_voltage = (uint16_t)rx_buf[0 + 0];
    raw_bse_front_voltage |= (uint16_t)(rx_buf[0 + 1] << 8);
    msg->bse_front_voltage = (float)raw_bse_front_voltage * 0.0001f;

    // Unpack: BSE Rear Voltage
    uint16_t raw_bse_rear_voltage = 0;
    raw_bse_rear_voltage = (uint16_t)rx_buf[2 + 0];
    raw_bse_rear_voltage |= (uint16_t)(rx_buf[2 + 1] << 8);
    msg->bse_rear_voltage = (float)raw_bse_rear_voltage * 0.0001f;

    // Unpack: BSE Line Lock Voltage
    uint16_t raw_bse_line_lock_voltage = 0;
    raw_bse_line_lock_voltage = (uint16_t)rx_buf[4 + 0];
    raw_bse_line_lock_voltage |= (uint16_t)(rx_buf[4 + 1] << 8);
    msg->bse_line_lock_voltage = (float)raw_bse_line_lock_voltage * 0.0001f;

    return 0;
}

// Packet: Brakes
int pack_brakes(const msg_brakes_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, BRAKES_DLC);

    // Pack: Brake Pressure Front
    uint16_t raw_brake_pressure_front = (uint16_t)(msg->brake_pressure_front / 0.05f);
    tx_buf[0 + 0] = (uint8_t)(raw_brake_pressure_front & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_brake_pressure_front >> 8) & 0xFF);

    // Pack: Brake Pressure Rear Pre Lock
    uint16_t raw_brake_pressure_rear_pre_lock = (uint16_t)(msg->brake_pressure_rear_pre_lock / 0.05f);
    tx_buf[2 + 0] = (uint8_t)(raw_brake_pressure_rear_pre_lock & 0xFF);
    tx_buf[2 + 1] = (uint8_t)((raw_brake_pressure_rear_pre_lock >> 8) & 0xFF);

    // Pack: Brake Pressure Rear Post Lock
    uint16_t raw_brake_pressure_rear_post_lock = (uint16_t)(msg->brake_pressure_rear_post_lock / 0.05f);
    tx_buf[4 + 0] = (uint8_t)(raw_brake_pressure_rear_post_lock & 0xFF);
    tx_buf[4 + 1] = (uint8_t)((raw_brake_pressure_rear_post_lock >> 8) & 0xFF);

    // Pack: Brake Bias
    uint8_t raw_brake_bias = (uint8_t)(msg->brake_bias / 0.01f);
    tx_buf[6] = (uint8_t)raw_brake_bias;

    // Pack: BSE Faults
    tx_buf[7] = (uint8_t)msg->bse_faults;

    return 0;
}

int unpack_brakes(const uint8_t* rx_buf, msg_brakes_t* msg) {
    // Unpack: Brake Pressure Front
    uint16_t raw_brake_pressure_front = 0;
    raw_brake_pressure_front = (uint16_t)rx_buf[0 + 0];
    raw_brake_pressure_front |= (uint16_t)(rx_buf[0 + 1] << 8);
    msg->brake_pressure_front = (float)raw_brake_pressure_front * 0.05f;

    // Unpack: Brake Pressure Rear Pre Lock
    uint16_t raw_brake_pressure_rear_pre_lock = 0;
    raw_brake_pressure_rear_pre_lock = (uint16_t)rx_buf[2 + 0];
    raw_brake_pressure_rear_pre_lock |= (uint16_t)(rx_buf[2 + 1] << 8);
    msg->brake_pressure_rear_pre_lock = (float)raw_brake_pressure_rear_pre_lock * 0.05f;

    // Unpack: Brake Pressure Rear Post Lock
    uint16_t raw_brake_pressure_rear_post_lock = 0;
    raw_brake_pressure_rear_post_lock = (uint16_t)rx_buf[4 + 0];
    raw_brake_pressure_rear_post_lock |= (uint16_t)(rx_buf[4 + 1] << 8);
    msg->brake_pressure_rear_post_lock = (float)raw_brake_pressure_rear_post_lock * 0.05f;

    // Unpack: Brake Bias
    uint8_t raw_brake_bias = 0;
    raw_brake_bias = (uint8_t)rx_buf[6];
    msg->brake_bias = (float)raw_brake_bias * 0.01f;

    // Unpack: BSE Faults
    msg->bse_faults = (uint8_t)rx_buf[7];

    return 0;
}

// Packet: Steering Column
int pack_steering_column(const msg_steering_column_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, STEERING_COLUMN_DLC);

    // Pack: Steering Column Angle
    int16_t raw_steering_column_angle = (int16_t)(msg->steering_column_angle / 0.004f);
    tx_buf[0 + 0] = (uint8_t)(raw_steering_column_angle & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_steering_column_angle >> 8) & 0xFF);

    return 0;
}

int unpack_steering_column(const uint8_t* rx_buf, msg_steering_column_t* msg) {
    // Unpack: Steering Column Angle
    int16_t raw_steering_column_angle = 0;
    raw_steering_column_angle = (int16_t)rx_buf[0 + 0];
    raw_steering_column_angle |= (int16_t)(rx_buf[0 + 1] << 8);
    msg->steering_column_angle = (float)raw_steering_column_angle * 0.004f;

    return 0;
}

// Packet: VCU State
int pack_vcu_state(const msg_vcu_state_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, VCU_STATE_DLC);

    // Pack: PRNDL State
    tx_buf[0] = (uint8_t)msg->prndl_state;

    // Pack: STOMP Fault
    tx_buf[1] = (uint8_t)msg->stomp_fault;

    // Pack: Ready To Drive Buzzer
    tx_buf[2] = (uint8_t)msg->ready_to_drive_buzzer;

    return 0;
}

int unpack_vcu_state(const uint8_t* rx_buf, msg_vcu_state_t* msg) {
    // Unpack: PRNDL State
    msg->prndl_state = (uint8_t)rx_buf[0];

    // Unpack: STOMP Fault
    msg->stomp_fault = (uint8_t)rx_buf[1];

    // Unpack: Ready To Drive Buzzer
    msg->ready_to_drive_buzzer = (uint8_t)rx_buf[2];

    return 0;
}

// Packet: Wheel Speeds
int pack_wheel_speeds(const msg_wheel_speeds_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, WHEEL_SPEEDS_DLC);

    // Pack: Front Left Speed
    int16_t raw_front_left_speed = (int16_t)(msg->front_left_speed / 0.01f);
    tx_buf[0 + 0] = (uint8_t)(raw_front_left_speed & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_front_left_speed >> 8) & 0xFF);

    // Pack: Front Right Speed
    int16_t raw_front_right_speed = (int16_t)(msg->front_right_speed / 0.01f);
    tx_buf[2 + 0] = (uint8_t)(raw_front_right_speed & 0xFF);
    tx_buf[2 + 1] = (uint8_t)((raw_front_right_speed >> 8) & 0xFF);

    // Pack: Back Left Speed
    int16_t raw_back_left_speed = (int16_t)(msg->back_left_speed / 0.01f);
    tx_buf[4 + 0] = (uint8_t)(raw_back_left_speed & 0xFF);
    tx_buf[4 + 1] = (uint8_t)((raw_back_left_speed >> 8) & 0xFF);

    // Pack: Back Right Speed
    int16_t raw_back_right_speed = (int16_t)(msg->back_right_speed / 0.01f);
    tx_buf[6 + 0] = (uint8_t)(raw_back_right_speed & 0xFF);
    tx_buf[6 + 1] = (uint8_t)((raw_back_right_speed >> 8) & 0xFF);

    return 0;
}

int unpack_wheel_speeds(const uint8_t* rx_buf, msg_wheel_speeds_t* msg) {
    // Unpack: Front Left Speed
    int16_t raw_front_left_speed = 0;
    raw_front_left_speed = (int16_t)rx_buf[0 + 0];
    raw_front_left_speed |= (int16_t)(rx_buf[0 + 1] << 8);
    msg->front_left_speed = (float)raw_front_left_speed * 0.01f;

    // Unpack: Front Right Speed
    int16_t raw_front_right_speed = 0;
    raw_front_right_speed = (int16_t)rx_buf[2 + 0];
    raw_front_right_speed |= (int16_t)(rx_buf[2 + 1] << 8);
    msg->front_right_speed = (float)raw_front_right_speed * 0.01f;

    // Unpack: Back Left Speed
    int16_t raw_back_left_speed = 0;
    raw_back_left_speed = (int16_t)rx_buf[4 + 0];
    raw_back_left_speed |= (int16_t)(rx_buf[4 + 1] << 8);
    msg->back_left_speed = (float)raw_back_left_speed * 0.01f;

    // Unpack: Back Right Speed
    int16_t raw_back_right_speed = 0;
    raw_back_right_speed = (int16_t)rx_buf[6 + 0];
    raw_back_right_speed |= (int16_t)(rx_buf[6 + 1] << 8);
    msg->back_right_speed = (float)raw_back_right_speed * 0.01f;

    return 0;
}

// Packet: Acceleration Vector Unsprung FL
int pack_acceleration_vector_unsprung_fl(const msg_acceleration_vector_unsprung_fl_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, ACCELERATION_VECTOR_UNSPRUNG_FL_DLC);

    // Pack: X
    int16_t raw_x = (int16_t)(msg->x / 0.001f);
    tx_buf[0 + 0] = (uint8_t)(raw_x & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_x >> 8) & 0xFF);

    // Pack: Y
    int16_t raw_y = (int16_t)(msg->y / 0.001f);
    tx_buf[2 + 0] = (uint8_t)(raw_y & 0xFF);
    tx_buf[2 + 1] = (uint8_t)((raw_y >> 8) & 0xFF);

    // Pack: Z
    int16_t raw_z = (int16_t)(msg->z / 0.001f);
    tx_buf[4 + 0] = (uint8_t)(raw_z & 0xFF);
    tx_buf[4 + 1] = (uint8_t)((raw_z >> 8) & 0xFF);

    return 0;
}

int unpack_acceleration_vector_unsprung_fl(const uint8_t* rx_buf, msg_acceleration_vector_unsprung_fl_t* msg) {
    // Unpack: X
    int16_t raw_x = 0;
    raw_x = (int16_t)rx_buf[0 + 0];
    raw_x |= (int16_t)(rx_buf[0 + 1] << 8);
    msg->x = (float)raw_x * 0.001f;

    // Unpack: Y
    int16_t raw_y = 0;
    raw_y = (int16_t)rx_buf[2 + 0];
    raw_y |= (int16_t)(rx_buf[2 + 1] << 8);
    msg->y = (float)raw_y * 0.001f;

    // Unpack: Z
    int16_t raw_z = 0;
    raw_z = (int16_t)rx_buf[4 + 0];
    raw_z |= (int16_t)(rx_buf[4 + 1] << 8);
    msg->z = (float)raw_z * 0.001f;

    return 0;
}

// Packet: Acceleration Vector Unsprung FR
int pack_acceleration_vector_unsprung_fr(const msg_acceleration_vector_unsprung_fr_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, ACCELERATION_VECTOR_UNSPRUNG_FR_DLC);

    // Pack: X
    int16_t raw_x = (int16_t)(msg->x / 0.001f);
    tx_buf[0 + 0] = (uint8_t)(raw_x & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_x >> 8) & 0xFF);

    // Pack: Y
    int16_t raw_y = (int16_t)(msg->y / 0.001f);
    tx_buf[2 + 0] = (uint8_t)(raw_y & 0xFF);
    tx_buf[2 + 1] = (uint8_t)((raw_y >> 8) & 0xFF);

    // Pack: Z
    int16_t raw_z = (int16_t)(msg->z / 0.001f);
    tx_buf[4 + 0] = (uint8_t)(raw_z & 0xFF);
    tx_buf[4 + 1] = (uint8_t)((raw_z >> 8) & 0xFF);

    return 0;
}

int unpack_acceleration_vector_unsprung_fr(const uint8_t* rx_buf, msg_acceleration_vector_unsprung_fr_t* msg) {
    // Unpack: X
    int16_t raw_x = 0;
    raw_x = (int16_t)rx_buf[0 + 0];
    raw_x |= (int16_t)(rx_buf[0 + 1] << 8);
    msg->x = (float)raw_x * 0.001f;

    // Unpack: Y
    int16_t raw_y = 0;
    raw_y = (int16_t)rx_buf[2 + 0];
    raw_y |= (int16_t)(rx_buf[2 + 1] << 8);
    msg->y = (float)raw_y * 0.001f;

    // Unpack: Z
    int16_t raw_z = 0;
    raw_z = (int16_t)rx_buf[4 + 0];
    raw_z |= (int16_t)(rx_buf[4 + 1] << 8);
    msg->z = (float)raw_z * 0.001f;

    return 0;
}

// Packet: Acceleration Vector Unsprung RL
int pack_acceleration_vector_unsprung_rl(const msg_acceleration_vector_unsprung_rl_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, ACCELERATION_VECTOR_UNSPRUNG_RL_DLC);

    // Pack: X
    int16_t raw_x = (int16_t)(msg->x / 0.001f);
    tx_buf[0 + 0] = (uint8_t)(raw_x & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_x >> 8) & 0xFF);

    // Pack: Y
    int16_t raw_y = (int16_t)(msg->y / 0.001f);
    tx_buf[2 + 0] = (uint8_t)(raw_y & 0xFF);
    tx_buf[2 + 1] = (uint8_t)((raw_y >> 8) & 0xFF);

    // Pack: Z
    int16_t raw_z = (int16_t)(msg->z / 0.001f);
    tx_buf[4 + 0] = (uint8_t)(raw_z & 0xFF);
    tx_buf[4 + 1] = (uint8_t)((raw_z >> 8) & 0xFF);

    return 0;
}

int unpack_acceleration_vector_unsprung_rl(const uint8_t* rx_buf, msg_acceleration_vector_unsprung_rl_t* msg) {
    // Unpack: X
    int16_t raw_x = 0;
    raw_x = (int16_t)rx_buf[0 + 0];
    raw_x |= (int16_t)(rx_buf[0 + 1] << 8);
    msg->x = (float)raw_x * 0.001f;

    // Unpack: Y
    int16_t raw_y = 0;
    raw_y = (int16_t)rx_buf[2 + 0];
    raw_y |= (int16_t)(rx_buf[2 + 1] << 8);
    msg->y = (float)raw_y * 0.001f;

    // Unpack: Z
    int16_t raw_z = 0;
    raw_z = (int16_t)rx_buf[4 + 0];
    raw_z |= (int16_t)(rx_buf[4 + 1] << 8);
    msg->z = (float)raw_z * 0.001f;

    return 0;
}

// Packet: Acceleration Vector Unsprung RR
int pack_acceleration_vector_unsprung_rr(const msg_acceleration_vector_unsprung_rr_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, ACCELERATION_VECTOR_UNSPRUNG_RR_DLC);

    // Pack: X
    int16_t raw_x = (int16_t)(msg->x / 0.001f);
    tx_buf[0 + 0] = (uint8_t)(raw_x & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_x >> 8) & 0xFF);

    // Pack: Y
    int16_t raw_y = (int16_t)(msg->y / 0.001f);
    tx_buf[2 + 0] = (uint8_t)(raw_y & 0xFF);
    tx_buf[2 + 1] = (uint8_t)((raw_y >> 8) & 0xFF);

    // Pack: Z
    int16_t raw_z = (int16_t)(msg->z / 0.001f);
    tx_buf[4 + 0] = (uint8_t)(raw_z & 0xFF);
    tx_buf[4 + 1] = (uint8_t)((raw_z >> 8) & 0xFF);

    return 0;
}

int unpack_acceleration_vector_unsprung_rr(const uint8_t* rx_buf, msg_acceleration_vector_unsprung_rr_t* msg) {
    // Unpack: X
    int16_t raw_x = 0;
    raw_x = (int16_t)rx_buf[0 + 0];
    raw_x |= (int16_t)(rx_buf[0 + 1] << 8);
    msg->x = (float)raw_x * 0.001f;

    // Unpack: Y
    int16_t raw_y = 0;
    raw_y = (int16_t)rx_buf[2 + 0];
    raw_y |= (int16_t)(rx_buf[2 + 1] << 8);
    msg->y = (float)raw_y * 0.001f;

    // Unpack: Z
    int16_t raw_z = 0;
    raw_z = (int16_t)rx_buf[4 + 0];
    raw_z |= (int16_t)(rx_buf[4 + 1] << 8);
    msg->z = (float)raw_z * 0.001f;

    return 0;
}

// Packet: FL Accel + Ride Height
int pack_fl_accel_ride_height(const msg_fl_accel_ride_height_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, FL_ACCEL_RIDE_HEIGHT_DLC);

    // Pack: X
    int16_t raw_x = (int16_t)(msg->x / 0.001f);
    tx_buf[0 + 0] = (uint8_t)(raw_x & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_x >> 8) & 0xFF);

    // Pack: Y
    int16_t raw_y = (int16_t)(msg->y / 0.001f);
    tx_buf[2 + 0] = (uint8_t)(raw_y & 0xFF);
    tx_buf[2 + 1] = (uint8_t)((raw_y >> 8) & 0xFF);

    // Pack: Z
    int16_t raw_z = (int16_t)(msg->z / 0.001f);
    tx_buf[4 + 0] = (uint8_t)(raw_z & 0xFF);
    tx_buf[4 + 1] = (uint8_t)((raw_z >> 8) & 0xFF);

    // Pack: Ride Height
    uint16_t raw_ride_height = (uint16_t)(msg->ride_height / 0.002f);
    tx_buf[6 + 0] = (uint8_t)(raw_ride_height & 0xFF);
    tx_buf[6 + 1] = (uint8_t)((raw_ride_height >> 8) & 0xFF);

    return 0;
}

int unpack_fl_accel_ride_height(const uint8_t* rx_buf, msg_fl_accel_ride_height_t* msg) {
    // Unpack: X
    int16_t raw_x = 0;
    raw_x = (int16_t)rx_buf[0 + 0];
    raw_x |= (int16_t)(rx_buf[0 + 1] << 8);
    msg->x = (float)raw_x * 0.001f;

    // Unpack: Y
    int16_t raw_y = 0;
    raw_y = (int16_t)rx_buf[2 + 0];
    raw_y |= (int16_t)(rx_buf[2 + 1] << 8);
    msg->y = (float)raw_y * 0.001f;

    // Unpack: Z
    int16_t raw_z = 0;
    raw_z = (int16_t)rx_buf[4 + 0];
    raw_z |= (int16_t)(rx_buf[4 + 1] << 8);
    msg->z = (float)raw_z * 0.001f;

    // Unpack: Ride Height
    uint16_t raw_ride_height = 0;
    raw_ride_height = (uint16_t)rx_buf[6 + 0];
    raw_ride_height |= (uint16_t)(rx_buf[6 + 1] << 8);
    msg->ride_height = (float)raw_ride_height * 0.002f;

    return 0;
}

// Packet: FL Strain Gauge + Sus Pot.
int pack_fl_strain_gauge_sus_pot(const msg_fl_strain_gauge_sus_pot_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, FL_STRAIN_GAUGE_SUS_POT_DLC);

    // Pack: Front Left Strain Gauge Voltage
    int16_t raw_front_left_strain_gauge_voltage = (int16_t)(msg->front_left_strain_gauge_voltage / 0.0002f);
    tx_buf[0 + 0] = (uint8_t)(raw_front_left_strain_gauge_voltage & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_front_left_strain_gauge_voltage >> 8) & 0xFF);

    // Pack: Front Left Suspension Potentiometer
    int16_t raw_front_left_suspension_potentiometer = (int16_t)(msg->front_left_suspension_potentiometer / 0.001f);
    tx_buf[2 + 0] = (uint8_t)(raw_front_left_suspension_potentiometer & 0xFF);
    tx_buf[2 + 1] = (uint8_t)((raw_front_left_suspension_potentiometer >> 8) & 0xFF);

    return 0;
}

int unpack_fl_strain_gauge_sus_pot(const uint8_t* rx_buf, msg_fl_strain_gauge_sus_pot_t* msg) {
    // Unpack: Front Left Strain Gauge Voltage
    int16_t raw_front_left_strain_gauge_voltage = 0;
    raw_front_left_strain_gauge_voltage = (int16_t)rx_buf[0 + 0];
    raw_front_left_strain_gauge_voltage |= (int16_t)(rx_buf[0 + 1] << 8);
    msg->front_left_strain_gauge_voltage = (float)raw_front_left_strain_gauge_voltage * 0.0002f;

    // Unpack: Front Left Suspension Potentiometer
    int16_t raw_front_left_suspension_potentiometer = 0;
    raw_front_left_suspension_potentiometer = (int16_t)rx_buf[2 + 0];
    raw_front_left_suspension_potentiometer |= (int16_t)(rx_buf[2 + 1] << 8);
    msg->front_left_suspension_potentiometer = (float)raw_front_left_suspension_potentiometer * 0.001f;

    return 0;
}

// Packet: FR Accel + Ride Height
int pack_fr_accel_ride_height(const msg_fr_accel_ride_height_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, FR_ACCEL_RIDE_HEIGHT_DLC);

    // Pack: X
    int16_t raw_x = (int16_t)(msg->x / 0.001f);
    tx_buf[0 + 0] = (uint8_t)(raw_x & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_x >> 8) & 0xFF);

    // Pack: Y
    int16_t raw_y = (int16_t)(msg->y / 0.001f);
    tx_buf[2 + 0] = (uint8_t)(raw_y & 0xFF);
    tx_buf[2 + 1] = (uint8_t)((raw_y >> 8) & 0xFF);

    // Pack: Z
    int16_t raw_z = (int16_t)(msg->z / 0.001f);
    tx_buf[4 + 0] = (uint8_t)(raw_z & 0xFF);
    tx_buf[4 + 1] = (uint8_t)((raw_z >> 8) & 0xFF);

    // Pack: Ride Height
    uint16_t raw_ride_height = (uint16_t)(msg->ride_height / 0.002f);
    tx_buf[6 + 0] = (uint8_t)(raw_ride_height & 0xFF);
    tx_buf[6 + 1] = (uint8_t)((raw_ride_height >> 8) & 0xFF);

    return 0;
}

int unpack_fr_accel_ride_height(const uint8_t* rx_buf, msg_fr_accel_ride_height_t* msg) {
    // Unpack: X
    int16_t raw_x = 0;
    raw_x = (int16_t)rx_buf[0 + 0];
    raw_x |= (int16_t)(rx_buf[0 + 1] << 8);
    msg->x = (float)raw_x * 0.001f;

    // Unpack: Y
    int16_t raw_y = 0;
    raw_y = (int16_t)rx_buf[2 + 0];
    raw_y |= (int16_t)(rx_buf[2 + 1] << 8);
    msg->y = (float)raw_y * 0.001f;

    // Unpack: Z
    int16_t raw_z = 0;
    raw_z = (int16_t)rx_buf[4 + 0];
    raw_z |= (int16_t)(rx_buf[4 + 1] << 8);
    msg->z = (float)raw_z * 0.001f;

    // Unpack: Ride Height
    uint16_t raw_ride_height = 0;
    raw_ride_height = (uint16_t)rx_buf[6 + 0];
    raw_ride_height |= (uint16_t)(rx_buf[6 + 1] << 8);
    msg->ride_height = (float)raw_ride_height * 0.002f;

    return 0;
}

// Packet: FR Strain Gauge + Sus Pot.
int pack_fr_strain_gauge_sus_pot(const msg_fr_strain_gauge_sus_pot_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, FR_STRAIN_GAUGE_SUS_POT_DLC);

    // Pack: Front Right Strain Gauge Voltage
    int16_t raw_front_right_strain_gauge_voltage = (int16_t)(msg->front_right_strain_gauge_voltage / 0.0002f);
    tx_buf[0 + 0] = (uint8_t)(raw_front_right_strain_gauge_voltage & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_front_right_strain_gauge_voltage >> 8) & 0xFF);

    // Pack: Front Right Suspension Potentiometer
    int16_t raw_front_right_suspension_potentiometer = (int16_t)(msg->front_right_suspension_potentiometer / 0.001f);
    tx_buf[2 + 0] = (uint8_t)(raw_front_right_suspension_potentiometer & 0xFF);
    tx_buf[2 + 1] = (uint8_t)((raw_front_right_suspension_potentiometer >> 8) & 0xFF);

    return 0;
}

int unpack_fr_strain_gauge_sus_pot(const uint8_t* rx_buf, msg_fr_strain_gauge_sus_pot_t* msg) {
    // Unpack: Front Right Strain Gauge Voltage
    int16_t raw_front_right_strain_gauge_voltage = 0;
    raw_front_right_strain_gauge_voltage = (int16_t)rx_buf[0 + 0];
    raw_front_right_strain_gauge_voltage |= (int16_t)(rx_buf[0 + 1] << 8);
    msg->front_right_strain_gauge_voltage = (float)raw_front_right_strain_gauge_voltage * 0.0002f;

    // Unpack: Front Right Suspension Potentiometer
    int16_t raw_front_right_suspension_potentiometer = 0;
    raw_front_right_suspension_potentiometer = (int16_t)rx_buf[2 + 0];
    raw_front_right_suspension_potentiometer |= (int16_t)(rx_buf[2 + 1] << 8);
    msg->front_right_suspension_potentiometer = (float)raw_front_right_suspension_potentiometer * 0.001f;

    return 0;
}

// Packet: RL Accel + Ride Height
int pack_rl_accel_ride_height(const msg_rl_accel_ride_height_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, RL_ACCEL_RIDE_HEIGHT_DLC);

    // Pack: X
    int16_t raw_x = (int16_t)(msg->x / 0.001f);
    tx_buf[0 + 0] = (uint8_t)(raw_x & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_x >> 8) & 0xFF);

    // Pack: Y
    int16_t raw_y = (int16_t)(msg->y / 0.001f);
    tx_buf[2 + 0] = (uint8_t)(raw_y & 0xFF);
    tx_buf[2 + 1] = (uint8_t)((raw_y >> 8) & 0xFF);

    // Pack: Z
    int16_t raw_z = (int16_t)(msg->z / 0.001f);
    tx_buf[4 + 0] = (uint8_t)(raw_z & 0xFF);
    tx_buf[4 + 1] = (uint8_t)((raw_z >> 8) & 0xFF);

    // Pack: Ride Height
    uint16_t raw_ride_height = (uint16_t)(msg->ride_height / 0.002f);
    tx_buf[6 + 0] = (uint8_t)(raw_ride_height & 0xFF);
    tx_buf[6 + 1] = (uint8_t)((raw_ride_height >> 8) & 0xFF);

    return 0;
}

int unpack_rl_accel_ride_height(const uint8_t* rx_buf, msg_rl_accel_ride_height_t* msg) {
    // Unpack: X
    int16_t raw_x = 0;
    raw_x = (int16_t)rx_buf[0 + 0];
    raw_x |= (int16_t)(rx_buf[0 + 1] << 8);
    msg->x = (float)raw_x * 0.001f;

    // Unpack: Y
    int16_t raw_y = 0;
    raw_y = (int16_t)rx_buf[2 + 0];
    raw_y |= (int16_t)(rx_buf[2 + 1] << 8);
    msg->y = (float)raw_y * 0.001f;

    // Unpack: Z
    int16_t raw_z = 0;
    raw_z = (int16_t)rx_buf[4 + 0];
    raw_z |= (int16_t)(rx_buf[4 + 1] << 8);
    msg->z = (float)raw_z * 0.001f;

    // Unpack: Ride Height
    uint16_t raw_ride_height = 0;
    raw_ride_height = (uint16_t)rx_buf[6 + 0];
    raw_ride_height |= (uint16_t)(rx_buf[6 + 1] << 8);
    msg->ride_height = (float)raw_ride_height * 0.002f;

    return 0;
}

// Packet: RL Strain Gauge + Sus Pot.
int pack_rl_strain_gauge_sus_pot(const msg_rl_strain_gauge_sus_pot_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, RL_STRAIN_GAUGE_SUS_POT_DLC);

    // Pack: Back Left Strain Gauge Voltage
    int16_t raw_back_left_strain_gauge_voltage = (int16_t)(msg->back_left_strain_gauge_voltage / 0.0002f);
    tx_buf[0 + 0] = (uint8_t)(raw_back_left_strain_gauge_voltage & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_back_left_strain_gauge_voltage >> 8) & 0xFF);

    // Pack: Back Left Suspension Potentiometer
    int16_t raw_back_left_suspension_potentiometer = (int16_t)(msg->back_left_suspension_potentiometer / 0.001f);
    tx_buf[2 + 0] = (uint8_t)(raw_back_left_suspension_potentiometer & 0xFF);
    tx_buf[2 + 1] = (uint8_t)((raw_back_left_suspension_potentiometer >> 8) & 0xFF);

    return 0;
}

int unpack_rl_strain_gauge_sus_pot(const uint8_t* rx_buf, msg_rl_strain_gauge_sus_pot_t* msg) {
    // Unpack: Back Left Strain Gauge Voltage
    int16_t raw_back_left_strain_gauge_voltage = 0;
    raw_back_left_strain_gauge_voltage = (int16_t)rx_buf[0 + 0];
    raw_back_left_strain_gauge_voltage |= (int16_t)(rx_buf[0 + 1] << 8);
    msg->back_left_strain_gauge_voltage = (float)raw_back_left_strain_gauge_voltage * 0.0002f;

    // Unpack: Back Left Suspension Potentiometer
    int16_t raw_back_left_suspension_potentiometer = 0;
    raw_back_left_suspension_potentiometer = (int16_t)rx_buf[2 + 0];
    raw_back_left_suspension_potentiometer |= (int16_t)(rx_buf[2 + 1] << 8);
    msg->back_left_suspension_potentiometer = (float)raw_back_left_suspension_potentiometer * 0.001f;

    return 0;
}

// Packet: RR Accel + Ride Height
int pack_rr_accel_ride_height(const msg_rr_accel_ride_height_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, RR_ACCEL_RIDE_HEIGHT_DLC);

    // Pack: X
    int16_t raw_x = (int16_t)(msg->x / 0.001f);
    tx_buf[0 + 0] = (uint8_t)(raw_x & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_x >> 8) & 0xFF);

    // Pack: Y
    int16_t raw_y = (int16_t)(msg->y / 0.001f);
    tx_buf[2 + 0] = (uint8_t)(raw_y & 0xFF);
    tx_buf[2 + 1] = (uint8_t)((raw_y >> 8) & 0xFF);

    // Pack: Z
    int16_t raw_z = (int16_t)(msg->z / 0.001f);
    tx_buf[4 + 0] = (uint8_t)(raw_z & 0xFF);
    tx_buf[4 + 1] = (uint8_t)((raw_z >> 8) & 0xFF);

    // Pack: Ride Height
    uint16_t raw_ride_height = (uint16_t)(msg->ride_height / 0.002f);
    tx_buf[6 + 0] = (uint8_t)(raw_ride_height & 0xFF);
    tx_buf[6 + 1] = (uint8_t)((raw_ride_height >> 8) & 0xFF);

    return 0;
}

int unpack_rr_accel_ride_height(const uint8_t* rx_buf, msg_rr_accel_ride_height_t* msg) {
    // Unpack: X
    int16_t raw_x = 0;
    raw_x = (int16_t)rx_buf[0 + 0];
    raw_x |= (int16_t)(rx_buf[0 + 1] << 8);
    msg->x = (float)raw_x * 0.001f;

    // Unpack: Y
    int16_t raw_y = 0;
    raw_y = (int16_t)rx_buf[2 + 0];
    raw_y |= (int16_t)(rx_buf[2 + 1] << 8);
    msg->y = (float)raw_y * 0.001f;

    // Unpack: Z
    int16_t raw_z = 0;
    raw_z = (int16_t)rx_buf[4 + 0];
    raw_z |= (int16_t)(rx_buf[4 + 1] << 8);
    msg->z = (float)raw_z * 0.001f;

    // Unpack: Ride Height
    uint16_t raw_ride_height = 0;
    raw_ride_height = (uint16_t)rx_buf[6 + 0];
    raw_ride_height |= (uint16_t)(rx_buf[6 + 1] << 8);
    msg->ride_height = (float)raw_ride_height * 0.002f;

    return 0;
}

// Packet: RR Strain Gauge + Sus Pot.
int pack_rr_strain_gauge_sus_pot(const msg_rr_strain_gauge_sus_pot_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, RR_STRAIN_GAUGE_SUS_POT_DLC);

    // Pack: Back Right Strain Gauge Voltage
    int16_t raw_back_right_strain_gauge_voltage = (int16_t)(msg->back_right_strain_gauge_voltage / 0.0002f);
    tx_buf[0 + 0] = (uint8_t)(raw_back_right_strain_gauge_voltage & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_back_right_strain_gauge_voltage >> 8) & 0xFF);

    // Pack: Back Right Suspension Potentiometer
    int16_t raw_back_right_suspension_potentiometer = (int16_t)(msg->back_right_suspension_potentiometer / 0.001f);
    tx_buf[2 + 0] = (uint8_t)(raw_back_right_suspension_potentiometer & 0xFF);
    tx_buf[2 + 1] = (uint8_t)((raw_back_right_suspension_potentiometer >> 8) & 0xFF);

    return 0;
}

int unpack_rr_strain_gauge_sus_pot(const uint8_t* rx_buf, msg_rr_strain_gauge_sus_pot_t* msg) {
    // Unpack: Back Right Strain Gauge Voltage
    int16_t raw_back_right_strain_gauge_voltage = 0;
    raw_back_right_strain_gauge_voltage = (int16_t)rx_buf[0 + 0];
    raw_back_right_strain_gauge_voltage |= (int16_t)(rx_buf[0 + 1] << 8);
    msg->back_right_strain_gauge_voltage = (float)raw_back_right_strain_gauge_voltage * 0.0002f;

    // Unpack: Back Right Suspension Potentiometer
    int16_t raw_back_right_suspension_potentiometer = 0;
    raw_back_right_suspension_potentiometer = (int16_t)rx_buf[2 + 0];
    raw_back_right_suspension_potentiometer |= (int16_t)(rx_buf[2 + 1] << 8);
    msg->back_right_suspension_potentiometer = (float)raw_back_right_suspension_potentiometer * 0.001f;

    return 0;
}

// Packet: FL Gyro
int pack_fl_gyro(const msg_fl_gyro_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, FL_GYRO_DLC);

    // Pack: X
    int16_t raw_x = (int16_t)(msg->x / 0.001f);
    tx_buf[0 + 0] = (uint8_t)(raw_x & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_x >> 8) & 0xFF);

    // Pack: Y
    int16_t raw_y = (int16_t)(msg->y / 0.001f);
    tx_buf[2 + 0] = (uint8_t)(raw_y & 0xFF);
    tx_buf[2 + 1] = (uint8_t)((raw_y >> 8) & 0xFF);

    // Pack: Z
    int16_t raw_z = (int16_t)(msg->z / 0.001f);
    tx_buf[4 + 0] = (uint8_t)(raw_z & 0xFF);
    tx_buf[4 + 1] = (uint8_t)((raw_z >> 8) & 0xFF);

    return 0;
}

int unpack_fl_gyro(const uint8_t* rx_buf, msg_fl_gyro_t* msg) {
    // Unpack: X
    int16_t raw_x = 0;
    raw_x = (int16_t)rx_buf[0 + 0];
    raw_x |= (int16_t)(rx_buf[0 + 1] << 8);
    msg->x = (float)raw_x * 0.001f;

    // Unpack: Y
    int16_t raw_y = 0;
    raw_y = (int16_t)rx_buf[2 + 0];
    raw_y |= (int16_t)(rx_buf[2 + 1] << 8);
    msg->y = (float)raw_y * 0.001f;

    // Unpack: Z
    int16_t raw_z = 0;
    raw_z = (int16_t)rx_buf[4 + 0];
    raw_z |= (int16_t)(rx_buf[4 + 1] << 8);
    msg->z = (float)raw_z * 0.001f;

    return 0;
}

// Packet: FR Gyro
int pack_fr_gyro(const msg_fr_gyro_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, FR_GYRO_DLC);

    // Pack: X
    int16_t raw_x = (int16_t)(msg->x / 0.001f);
    tx_buf[0 + 0] = (uint8_t)(raw_x & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_x >> 8) & 0xFF);

    // Pack: Y
    int16_t raw_y = (int16_t)(msg->y / 0.001f);
    tx_buf[2 + 0] = (uint8_t)(raw_y & 0xFF);
    tx_buf[2 + 1] = (uint8_t)((raw_y >> 8) & 0xFF);

    // Pack: Z
    int16_t raw_z = (int16_t)(msg->z / 0.001f);
    tx_buf[4 + 0] = (uint8_t)(raw_z & 0xFF);
    tx_buf[4 + 1] = (uint8_t)((raw_z >> 8) & 0xFF);

    return 0;
}

int unpack_fr_gyro(const uint8_t* rx_buf, msg_fr_gyro_t* msg) {
    // Unpack: X
    int16_t raw_x = 0;
    raw_x = (int16_t)rx_buf[0 + 0];
    raw_x |= (int16_t)(rx_buf[0 + 1] << 8);
    msg->x = (float)raw_x * 0.001f;

    // Unpack: Y
    int16_t raw_y = 0;
    raw_y = (int16_t)rx_buf[2 + 0];
    raw_y |= (int16_t)(rx_buf[2 + 1] << 8);
    msg->y = (float)raw_y * 0.001f;

    // Unpack: Z
    int16_t raw_z = 0;
    raw_z = (int16_t)rx_buf[4 + 0];
    raw_z |= (int16_t)(rx_buf[4 + 1] << 8);
    msg->z = (float)raw_z * 0.001f;

    return 0;
}

// Packet: RL Gyro
int pack_rl_gyro(const msg_rl_gyro_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, RL_GYRO_DLC);

    // Pack: X
    int16_t raw_x = (int16_t)(msg->x / 0.001f);
    tx_buf[0 + 0] = (uint8_t)(raw_x & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_x >> 8) & 0xFF);

    // Pack: Y
    int16_t raw_y = (int16_t)(msg->y / 0.001f);
    tx_buf[2 + 0] = (uint8_t)(raw_y & 0xFF);
    tx_buf[2 + 1] = (uint8_t)((raw_y >> 8) & 0xFF);

    // Pack: Z
    int16_t raw_z = (int16_t)(msg->z / 0.001f);
    tx_buf[4 + 0] = (uint8_t)(raw_z & 0xFF);
    tx_buf[4 + 1] = (uint8_t)((raw_z >> 8) & 0xFF);

    return 0;
}

int unpack_rl_gyro(const uint8_t* rx_buf, msg_rl_gyro_t* msg) {
    // Unpack: X
    int16_t raw_x = 0;
    raw_x = (int16_t)rx_buf[0 + 0];
    raw_x |= (int16_t)(rx_buf[0 + 1] << 8);
    msg->x = (float)raw_x * 0.001f;

    // Unpack: Y
    int16_t raw_y = 0;
    raw_y = (int16_t)rx_buf[2 + 0];
    raw_y |= (int16_t)(rx_buf[2 + 1] << 8);
    msg->y = (float)raw_y * 0.001f;

    // Unpack: Z
    int16_t raw_z = 0;
    raw_z = (int16_t)rx_buf[4 + 0];
    raw_z |= (int16_t)(rx_buf[4 + 1] << 8);
    msg->z = (float)raw_z * 0.001f;

    return 0;
}

// Packet: RR Gyro
int pack_rr_gyro(const msg_rr_gyro_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, RR_GYRO_DLC);

    // Pack: X
    int16_t raw_x = (int16_t)(msg->x / 0.001f);
    tx_buf[0 + 0] = (uint8_t)(raw_x & 0xFF);
    tx_buf[0 + 1] = (uint8_t)((raw_x >> 8) & 0xFF);

    // Pack: Y
    int16_t raw_y = (int16_t)(msg->y / 0.001f);
    tx_buf[2 + 0] = (uint8_t)(raw_y & 0xFF);
    tx_buf[2 + 1] = (uint8_t)((raw_y >> 8) & 0xFF);

    // Pack: Z
    int16_t raw_z = (int16_t)(msg->z / 0.001f);
    tx_buf[4 + 0] = (uint8_t)(raw_z & 0xFF);
    tx_buf[4 + 1] = (uint8_t)((raw_z >> 8) & 0xFF);

    return 0;
}

int unpack_rr_gyro(const uint8_t* rx_buf, msg_rr_gyro_t* msg) {
    // Unpack: X
    int16_t raw_x = 0;
    raw_x = (int16_t)rx_buf[0 + 0];
    raw_x |= (int16_t)(rx_buf[0 + 1] << 8);
    msg->x = (float)raw_x * 0.001f;

    // Unpack: Y
    int16_t raw_y = 0;
    raw_y = (int16_t)rx_buf[2 + 0];
    raw_y |= (int16_t)(rx_buf[2 + 1] << 8);
    msg->y = (float)raw_y * 0.001f;

    // Unpack: Z
    int16_t raw_z = 0;
    raw_z = (int16_t)rx_buf[4 + 0];
    raw_z |= (int16_t)(rx_buf[4 + 1] << 8);
    msg->z = (float)raw_z * 0.001f;

    return 0;
}

// Packet: VCU Enter Bootloader
int pack_vcu_enter_bootloader(const msg_vcu_enter_bootloader_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, VCU_ENTER_BOOTLOADER_DLC);

    return 0;
}

int unpack_vcu_enter_bootloader(const uint8_t* rx_buf, msg_vcu_enter_bootloader_t* msg) {
    return 0;
}

// Packet: CSM Enter Bootloader
int pack_csm_enter_bootloader(const msg_csm_enter_bootloader_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, CSM_ENTER_BOOTLOADER_DLC);

    return 0;
}

int unpack_csm_enter_bootloader(const uint8_t* rx_buf, msg_csm_enter_bootloader_t* msg) {
    return 0;
}

// Packet: HVC Enter Bootloader
int pack_hvc_enter_bootloader(const msg_hvc_enter_bootloader_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, HVC_ENTER_BOOTLOADER_DLC);

    return 0;
}

int unpack_hvc_enter_bootloader(const uint8_t* rx_buf, msg_hvc_enter_bootloader_t* msg) {
    return 0;
}

// Packet: USM Enter Bootloader
int pack_usm_enter_bootloader(const msg_usm_enter_bootloader_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, USM_ENTER_BOOTLOADER_DLC);

    return 0;
}

int unpack_usm_enter_bootloader(const uint8_t* rx_buf, msg_usm_enter_bootloader_t* msg) {
    return 0;
}

// Packet: HVC Bounds Parameters
int pack_hvc_bounds_parameters(const msg_hvc_bounds_parameters_t* msg, uint8_t* tx_buf) {
    memset(tx_buf, 0, HVC_BOUNDS_PARAMETERS_DLC);

    return 0;
}

int unpack_hvc_bounds_parameters(const uint8_t* rx_buf, msg_hvc_bounds_parameters_t* msg) {
    return 0;
}
