#ifndef CAN_IDS_H
#define CAN_IDS_H

// Auto-generated CAN packet definition header file
// Generated from: drivers/longhorn-lib/can.json
// DO NOT EDIT MANUALLY

#include <stdint.h>
#include <string.h>

// GCD of all packet frequencies
#define CAN_FREQ_GCD 1

// Generic Bitfield Manipulation Macros
// Extracts 'width' bits starting at 'start_bit' from 'value'
#define CAN_EXTRACT_BITFIELD(value, start_bit, width) \
    (((value) >> (start_bit)) & ((1ULL << (width)) - 1))

// Inserts 'field_val' into 'target' at 'start_bit' with 'width'
#define CAN_INSERT_BITFIELD(target, field_val, start_bit, width) do { \
    (target) &= ~(((1ULL << (width)) - 1) << (start_bit)); \
    (target) |= (((field_val) & ((1ULL << (width)) - 1)) << (start_bit)); \
} while(0)

// ==========================================================================
// Packet: Firmware Update Command Packet (16)
// ==========================================================================
// From: Pi
// To:   *
#define FIRMWARE_UPDATE_COMMAND_PACKET_ID 16
#define FIRMWARE_UPDATE_COMMAND_PACKET_DLC 8
#define FIRMWARE_UPDATE_COMMAND_PACKET_FREQ 0
#define FIRMWARE_UPDATE_COMMAND_PACKET_TIMEOUT_MS 0

typedef struct {
    uint8_t command;
    uint32_t address;
    uint16_t num_blocks;
    uint8_t crc;
} msg_firmware_update_command_packet_t;

// Signal: Command
#define FIRMWARE_UPDATE_COMMAND_PACKET_COMMAND_PREC 1.0f

// Signal: Address
#define FIRMWARE_UPDATE_COMMAND_PACKET_ADDRESS_PREC 1.0f

// Signal: Num Blocks
#define FIRMWARE_UPDATE_COMMAND_PACKET_NUM_BLOCKS_PREC 1.0f

// Signal: CRC
#define FIRMWARE_UPDATE_COMMAND_PACKET_CRC_PREC 1.0f

int pack_firmware_update_command_packet(const msg_firmware_update_command_packet_t* msg, uint8_t* tx_buf);
int unpack_firmware_update_command_packet(const uint8_t* rx_buf, msg_firmware_update_command_packet_t* msg);

// ==========================================================================
// Packet: Bus Enable/Disable (17)
// ==========================================================================
// From: Pi
// To:   HVC, VCU, USM, CSM
#define BUS_ENABLE_DISABLE_ID 17
#define BUS_ENABLE_DISABLE_DLC 8
#define BUS_ENABLE_DISABLE_FREQ 0
#define BUS_ENABLE_DISABLE_TIMEOUT_MS 0

typedef struct {
    uint8_t enable;
    uint8_t fw_update;
    uint8_t device;
} msg_bus_enable_disable_t;

// Signal: Enable
#define BUS_ENABLE_DISABLE_ENABLE_PREC 1.0f

// Signal: FW Update
#define BUS_ENABLE_DISABLE_FW_UPDATE_PREC 1.0f

// Signal: Device
#define BUS_ENABLE_DISABLE_DEVICE_PREC 1.0f

int pack_bus_enable_disable(const msg_bus_enable_disable_t* msg, uint8_t* tx_buf);
int unpack_bus_enable_disable(const uint8_t* rx_buf, msg_bus_enable_disable_t* msg);

// ==========================================================================
// Packet: Firmware Update Data Packet (18)
// ==========================================================================
// From: Pi
// To:   *
#define FIRMWARE_UPDATE_DATA_PACKET_ID 18
#define FIRMWARE_UPDATE_DATA_PACKET_DLC 8
#define FIRMWARE_UPDATE_DATA_PACKET_FREQ 0
#define FIRMWARE_UPDATE_DATA_PACKET_TIMEOUT_MS 0

typedef struct {
    uint8_t index;
    uint8_t data0;
    uint8_t data1;
    uint8_t data2;
    uint8_t data3;
    uint8_t data4;
    uint8_t data5;
    uint8_t data6;
} msg_firmware_update_data_packet_t;

// Signal: Index
#define FIRMWARE_UPDATE_DATA_PACKET_INDEX_PREC 1.0f

// Signal: data0
#define FIRMWARE_UPDATE_DATA_PACKET_DATA0_PREC 1.0f

// Signal: data1
#define FIRMWARE_UPDATE_DATA_PACKET_DATA1_PREC 1.0f

// Signal: data2
#define FIRMWARE_UPDATE_DATA_PACKET_DATA2_PREC 1.0f

// Signal: data3
#define FIRMWARE_UPDATE_DATA_PACKET_DATA3_PREC 1.0f

// Signal: data4
#define FIRMWARE_UPDATE_DATA_PACKET_DATA4_PREC 1.0f

// Signal: data5
#define FIRMWARE_UPDATE_DATA_PACKET_DATA5_PREC 1.0f

// Signal: data6
#define FIRMWARE_UPDATE_DATA_PACKET_DATA6_PREC 1.0f

int pack_firmware_update_data_packet(const msg_firmware_update_data_packet_t* msg, uint8_t* tx_buf);
int unpack_firmware_update_data_packet(const uint8_t* rx_buf, msg_firmware_update_data_packet_t* msg);

// ==========================================================================
// Packet: Device Firmware Update Response Packet (19)
// ==========================================================================
// From: Pi
// To:   *
#define DEVICE_FIRMWARE_UPDATE_RESPONSE_PACKET_ID 19
#define DEVICE_FIRMWARE_UPDATE_RESPONSE_PACKET_DLC 1
#define DEVICE_FIRMWARE_UPDATE_RESPONSE_PACKET_FREQ 0
#define DEVICE_FIRMWARE_UPDATE_RESPONSE_PACKET_TIMEOUT_MS 0

typedef struct {
    uint8_t response;
} msg_device_firmware_update_response_packet_t;

// Signal: Response
#define DEVICE_FIRMWARE_UPDATE_RESPONSE_PACKET_RESPONSE_PREC 1.0f

int pack_device_firmware_update_response_packet(const msg_device_firmware_update_response_packet_t* msg, uint8_t* tx_buf);
int unpack_device_firmware_update_response_packet(const uint8_t* rx_buf, msg_device_firmware_update_response_packet_t* msg);

// ==========================================================================
// Packet: Inverter Temps (160)
// ==========================================================================
// From: Inverter
// To:   VCU
#define INVERTER_TEMPS_ID 160
#define INVERTER_TEMPS_DLC 8
#define INVERTER_TEMPS_FREQ 100
#define INVERTER_TEMPS_TIMEOUT_MS 200

typedef struct {
    float module_a_temp;
    float module_b_temp;
    float module_c_temp;
    float gate_driver_temp;
} msg_inverter_temps_t;

// Signal: Module A Temp
#define INVERTER_TEMPS_MODULE_A_TEMP_PREC 0.1f

// Signal: Module B Temp
#define INVERTER_TEMPS_MODULE_B_TEMP_PREC 0.1f

// Signal: Module C Temp
#define INVERTER_TEMPS_MODULE_C_TEMP_PREC 0.1f

// Signal: Gate Driver Temp
#define INVERTER_TEMPS_GATE_DRIVER_TEMP_PREC 0.1f

int pack_inverter_temps(const msg_inverter_temps_t* msg, uint8_t* tx_buf);
int unpack_inverter_temps(const uint8_t* rx_buf, msg_inverter_temps_t* msg);

// ==========================================================================
// Packet: Inverter Temps 2 (162)
// ==========================================================================
// From: Inverter
// To:   VCU
#define INVERTER_TEMPS_2_ID 162
#define INVERTER_TEMPS_2_DLC 8
#define INVERTER_TEMPS_2_FREQ 100
#define INVERTER_TEMPS_2_TIMEOUT_MS 200

typedef struct {
    float coolant_temp;
    float inverter_hot_spot_temp;
    float motor_temp;
    float torque_shudder;
} msg_inverter_temps_2_t;

// Signal: Coolant Temp
#define INVERTER_TEMPS_2_COOLANT_TEMP_PREC 0.1f

// Signal: Inverter Hot Spot Temp
#define INVERTER_TEMPS_2_INVERTER_HOT_SPOT_TEMP_PREC 0.1f

// Signal: Motor Temp
#define INVERTER_TEMPS_2_MOTOR_TEMP_PREC 0.1f

// Signal: Torque Shudder
#define INVERTER_TEMPS_2_TORQUE_SHUDDER_PREC 0.1f

int pack_inverter_temps_2(const msg_inverter_temps_2_t* msg, uint8_t* tx_buf);
int unpack_inverter_temps_2(const uint8_t* rx_buf, msg_inverter_temps_2_t* msg);

// ==========================================================================
// Packet: Inverter Status (165)
// ==========================================================================
// From: Inverter
// To:   VCU
#define INVERTER_STATUS_ID 165
#define INVERTER_STATUS_DLC 8
#define INVERTER_STATUS_FREQ 10
#define INVERTER_STATUS_TIMEOUT_MS 20

typedef struct {
    float motor_angle;
    int16_t motor_speed;
    float inverter_frequency;
    float delta_resolver_angle;
} msg_inverter_status_t;

// Signal: Motor Angle
#define INVERTER_STATUS_MOTOR_ANGLE_PREC 0.1f

// Signal: Motor Speed
#define INVERTER_STATUS_MOTOR_SPEED_PREC 1.0f

// Signal: Inverter Frequency
#define INVERTER_STATUS_INVERTER_FREQUENCY_PREC 0.1f

// Signal: Delta Resolver Angle
#define INVERTER_STATUS_DELTA_RESOLVER_ANGLE_PREC 0.1f

int pack_inverter_status(const msg_inverter_status_t* msg, uint8_t* tx_buf);
int unpack_inverter_status(const uint8_t* rx_buf, msg_inverter_status_t* msg);

// ==========================================================================
// Packet: Inverter Current (166)
// ==========================================================================
// From: Inverter
// To:   VCU
#define INVERTER_CURRENT_ID 166
#define INVERTER_CURRENT_DLC 8
#define INVERTER_CURRENT_FREQ 100
#define INVERTER_CURRENT_TIMEOUT_MS 200

typedef struct {
    float phase_a_current;
    float phase_b_current;
    float phase_c_current;
    float dc_bus_current;
} msg_inverter_current_t;

// Signal: Phase A Current
#define INVERTER_CURRENT_PHASE_A_CURRENT_PREC 0.1f

// Signal: Phase B Current
#define INVERTER_CURRENT_PHASE_B_CURRENT_PREC 0.1f

// Signal: Phase C Current
#define INVERTER_CURRENT_PHASE_C_CURRENT_PREC 0.1f

// Signal: DC Bus Current
#define INVERTER_CURRENT_DC_BUS_CURRENT_PREC 0.1f

int pack_inverter_current(const msg_inverter_current_t* msg, uint8_t* tx_buf);
int unpack_inverter_current(const uint8_t* rx_buf, msg_inverter_current_t* msg);

// ==========================================================================
// Packet: Inverter Voltage (167)
// ==========================================================================
// From: Inverter
// To:   VCU
#define INVERTER_VOLTAGE_ID 167
#define INVERTER_VOLTAGE_DLC 8
#define INVERTER_VOLTAGE_FREQ 10
#define INVERTER_VOLTAGE_TIMEOUT_MS 20

typedef struct {
    float dc_bus_voltage;
    float neutral_output_voltage;
    float vab_vq_voltage;
    float vbc_vd_voltage;
} msg_inverter_voltage_t;

// Signal: DC Bus Voltage
#define INVERTER_VOLTAGE_DC_BUS_VOLTAGE_PREC 0.1f

// Signal: Neutral Output Voltage
#define INVERTER_VOLTAGE_NEUTRAL_OUTPUT_VOLTAGE_PREC 0.1f

// Signal: Vab / Vq Voltage
#define INVERTER_VOLTAGE_VAB_VQ_VOLTAGE_PREC 0.1f

// Signal: Vbc / Vd Voltage
#define INVERTER_VOLTAGE_VBC_VD_VOLTAGE_PREC 0.1f

int pack_inverter_voltage(const msg_inverter_voltage_t* msg, uint8_t* tx_buf);
int unpack_inverter_voltage(const uint8_t* rx_buf, msg_inverter_voltage_t* msg);

// ==========================================================================
// Packet: Inverter Details (170)
// ==========================================================================
// From: Inverter
// To:   VCU
#define INVERTER_DETAILS_ID 170
#define INVERTER_DETAILS_DLC 8
#define INVERTER_DETAILS_FREQ 10
#define INVERTER_DETAILS_TIMEOUT_MS 20

typedef struct {
    uint8_t vsm;
    uint8_t pwm_freq;
    uint8_t inverter;
    uint8_t relay;
    uint8_t misc_1;
    uint8_t misc_2;
    uint8_t misc_3;
    uint8_t misc_4;
} msg_inverter_details_t;

// Signal: VSM
#define INVERTER_DETAILS_VSM_PREC 1.0f

// Signal: PWM Freq
#define INVERTER_DETAILS_PWM_FREQ_PREC 1.0f

// Signal: Inverter
#define INVERTER_DETAILS_INVERTER_PREC 1.0f

// Signal: Relay
#define INVERTER_DETAILS_RELAY_PREC 1.0f

// Signal: Misc. 1
#define INVERTER_DETAILS_MISC_1_PREC 1.0f

// Signal: Misc. 2
#define INVERTER_DETAILS_MISC_2_PREC 1.0f

// Signal: Misc. 3
#define INVERTER_DETAILS_MISC_3_PREC 1.0f

// Signal: Misc. 4
#define INVERTER_DETAILS_MISC_4_PREC 1.0f

int pack_inverter_details(const msg_inverter_details_t* msg, uint8_t* tx_buf);
int unpack_inverter_details(const uint8_t* rx_buf, msg_inverter_details_t* msg);

// ==========================================================================
// Packet: Inverter Faults (171)
// ==========================================================================
// From: Inverter
// To:   VCU
#define INVERTER_FAULTS_ID 171
#define INVERTER_FAULTS_DLC 8
#define INVERTER_FAULTS_FREQ 10
#define INVERTER_FAULTS_TIMEOUT_MS 20

typedef struct {
    uint32_t post_faults;
    uint32_t run_faults;
} msg_inverter_faults_t;

// Signal: POST Faults
#define INVERTER_FAULTS_POST_FAULTS_PREC 1.0f

// Signal: Run Faults
#define INVERTER_FAULTS_RUN_FAULTS_PREC 1.0f

int pack_inverter_faults(const msg_inverter_faults_t* msg, uint8_t* tx_buf);
int unpack_inverter_faults(const uint8_t* rx_buf, msg_inverter_faults_t* msg);

// ==========================================================================
// Packet: Inverter TSO (172)
// ==========================================================================
// From: Inverter
// To:   VCU
#define INVERTER_TSO_ID 172
#define INVERTER_TSO_DLC 8
#define INVERTER_TSO_FREQ 10
#define INVERTER_TSO_TIMEOUT_MS 20

typedef struct {
    float commanded_torque;
    float torque_feedback;
    float time_since_turned_on;
} msg_inverter_tso_t;

// Signal: commanded torque
#define INVERTER_TSO_COMMANDED_TORQUE_PREC 0.1f

// Signal: torque feedback
#define INVERTER_TSO_TORQUE_FEEDBACK_PREC 0.1f

// Signal: Time since turned ON
#define INVERTER_TSO_TIME_SINCE_TURNED_ON_PREC 0.003f

int pack_inverter_tso(const msg_inverter_tso_t* msg, uint8_t* tx_buf);
int unpack_inverter_tso(const uint8_t* rx_buf, msg_inverter_tso_t* msg);

// ==========================================================================
// Packet: Inverter Speed (176)
// ==========================================================================
// From: Inverter
// To:   VCU
#define INVERTER_SPEED_ID 176
#define INVERTER_SPEED_DLC 8
#define INVERTER_SPEED_FREQ 3
#define INVERTER_SPEED_TIMEOUT_MS 6

typedef struct {
    float commanded_torque;
    float torque_feedback;
    int16_t motor_speed;
    float bus_voltage;
} msg_inverter_speed_t;

// Signal: commanded torque
#define INVERTER_SPEED_COMMANDED_TORQUE_PREC 0.1f

// Signal: torque feedback
#define INVERTER_SPEED_TORQUE_FEEDBACK_PREC 0.1f

// Signal: Motor Speed
#define INVERTER_SPEED_MOTOR_SPEED_PREC 1.0f

// Signal: Bus Voltage
#define INVERTER_SPEED_BUS_VOLTAGE_PREC 0.1f

int pack_inverter_speed(const msg_inverter_speed_t* msg, uint8_t* tx_buf);
int unpack_inverter_speed(const uint8_t* rx_buf, msg_inverter_speed_t* msg);

// ==========================================================================
// Packet: Inverter Torque Command (192)
// ==========================================================================
// From: VCU
// To:   Inverter
#define INVERTER_TORQUE_COMMAND_ID 192
#define INVERTER_TORQUE_COMMAND_DLC 8
#define INVERTER_TORQUE_COMMAND_FREQ 3
#define INVERTER_TORQUE_COMMAND_TIMEOUT_MS 6

typedef struct {
    float torque_request;
    int16_t rpm_request;
    uint8_t direction;
    uint8_t enable;
    float torque_limit;
} msg_inverter_torque_command_t;

// Signal: torque request
#define INVERTER_TORQUE_COMMAND_TORQUE_REQUEST_PREC 0.1f

// Signal: rpm request
#define INVERTER_TORQUE_COMMAND_RPM_REQUEST_PREC 1.0f

// Signal: direction
#define INVERTER_TORQUE_COMMAND_DIRECTION_PREC 1.0f

// Signal: enable
#define INVERTER_TORQUE_COMMAND_ENABLE_PREC 1.0f

// Signal: torque limit
#define INVERTER_TORQUE_COMMAND_TORQUE_LIMIT_PREC 0.1f

int pack_inverter_torque_command(const msg_inverter_torque_command_t* msg, uint8_t* tx_buf);
int unpack_inverter_torque_command(const uint8_t* rx_buf, msg_inverter_torque_command_t* msg);

// ==========================================================================
// Packet: Inverter Parameter Request (193)
// ==========================================================================
// From: VCU
// To:   Inverter
#define INVERTER_PARAMETER_REQUEST_ID 193
#define INVERTER_PARAMETER_REQUEST_DLC 8
#define INVERTER_PARAMETER_REQUEST_FREQ 0
#define INVERTER_PARAMETER_REQUEST_TIMEOUT_MS 0

typedef struct {
    uint16_t parameter_address;
    uint8_t r_w;
} msg_inverter_parameter_request_t;

// Signal: parameter address
#define INVERTER_PARAMETER_REQUEST_PARAMETER_ADDRESS_PREC 1.0f

// Signal: r/w
#define INVERTER_PARAMETER_REQUEST_R_W_PREC 1.0f

int pack_inverter_parameter_request(const msg_inverter_parameter_request_t* msg, uint8_t* tx_buf);
int unpack_inverter_parameter_request(const uint8_t* rx_buf, msg_inverter_parameter_request_t* msg);

// ==========================================================================
// Packet: Inverter Parameter Response (194)
// ==========================================================================
// From: Inverter
// To:   VCU
#define INVERTER_PARAMETER_RESPONSE_ID 194
#define INVERTER_PARAMETER_RESPONSE_DLC 8
#define INVERTER_PARAMETER_RESPONSE_FREQ 0
#define INVERTER_PARAMETER_RESPONSE_TIMEOUT_MS 0

typedef struct {
    uint16_t parameter_address;
    uint8_t success;
} msg_inverter_parameter_response_t;

// Signal: parameter address
#define INVERTER_PARAMETER_RESPONSE_PARAMETER_ADDRESS_PREC 1.0f

// Signal: success
#define INVERTER_PARAMETER_RESPONSE_SUCCESS_PREC 1.0f

int pack_inverter_parameter_response(const msg_inverter_parameter_response_t* msg, uint8_t* tx_buf);
int unpack_inverter_parameter_response(const uint8_t* rx_buf, msg_inverter_parameter_response_t* msg);

// ==========================================================================
// Packet: Cell Voltages (208)
// ==========================================================================
// From: HVC
// To:   PDU
#define CELL_VOLTAGES_ID 208
#define CELL_VOLTAGES_DLC 8
#define CELL_VOLTAGES_FREQ 1000
#define CELL_VOLTAGES_TIMEOUT_MS 2000

typedef struct {
    float voltage_i;
    float voltage_i_1;
    float voltage_i_2;
    float voltage_i_3;
} msg_cell_voltages_t;

// Signal: Voltage[i]
#define CELL_VOLTAGES_VOLTAGE_I_PREC 0.0001f

// Signal: Voltage[i+1]
#define CELL_VOLTAGES_VOLTAGE_I_1_PREC 0.0001f

// Signal: Voltage[i+2]
#define CELL_VOLTAGES_VOLTAGE_I_2_PREC 0.0001f

// Signal: Voltage[i+3]
#define CELL_VOLTAGES_VOLTAGE_I_3_PREC 0.0001f

int pack_cell_voltages(const msg_cell_voltages_t* msg, uint8_t* tx_buf);
int unpack_cell_voltages(const uint8_t* rx_buf, msg_cell_voltages_t* msg);

// ==========================================================================
// Packet: Cell Temperatures (256)
// ==========================================================================
// From: HVC
// To:   PDU
#define CELL_TEMPERATURES_ID 256
#define CELL_TEMPERATURES_DLC 8
#define CELL_TEMPERATURES_FREQ 1000
#define CELL_TEMPERATURES_TIMEOUT_MS 2000

typedef struct {
    float temp_i;
    float temp_i_1;
    float temp_i_2;
    float temp_i_3;
} msg_cell_temperatures_t;

// Signal: Temp[i]
#define CELL_TEMPERATURES_TEMP_I_PREC 0.1f

// Signal: Temp[i+1]
#define CELL_TEMPERATURES_TEMP_I_1_PREC 0.1f

// Signal: Temp[i+2]
#define CELL_TEMPERATURES_TEMP_I_2_PREC 0.1f

// Signal: Temp[i+3]
#define CELL_TEMPERATURES_TEMP_I_3_PREC 0.1f

int pack_cell_temperatures(const msg_cell_temperatures_t* msg, uint8_t* tx_buf);
int unpack_cell_temperatures(const uint8_t* rx_buf, msg_cell_temperatures_t* msg);

// ==========================================================================
// Packet: DUI R2D Status (288)
// ==========================================================================
// From: DUI
// To:   VCU
#define DUI_R2D_STATUS_ID 288
#define DUI_R2D_STATUS_DLC 2
#define DUI_R2D_STATUS_FREQ 100
#define DUI_R2D_STATUS_TIMEOUT_MS 200

typedef struct {
    uint8_t r2d_status;
    // Could not determine type for signal: temp_shutdown_1
    // Could not determine type for signal: temp_shutdown_2
    uint8_t dui_shutdown_faults;
} msg_dui_r2d_status_t;

// Signal: R2D Status
#define DUI_R2D_STATUS_R2D_STATUS_PREC 1.0f



// Bitfield Indices for: DUI Shutdown Faults
#define DUI_R2D_STATUS_DUI_SHUTDOWN_FAULTS_TEMP_SHUTDOWN_1_IDX 0
#define DUI_R2D_STATUS_DUI_SHUTDOWN_FAULTS_TEMP_SHUTDOWN_2_IDX 1

int pack_dui_r2d_status(const msg_dui_r2d_status_t* msg, uint8_t* tx_buf);
int unpack_dui_r2d_status(const uint8_t* rx_buf, msg_dui_r2d_status_t* msg);

// ==========================================================================
// Packet: DUI R2D Authorization (289)
// ==========================================================================
// From: VCU
// To:   DUI
#define DUI_R2D_AUTHORIZATION_ID 289
#define DUI_R2D_AUTHORIZATION_DLC 3
#define DUI_R2D_AUTHORIZATION_FREQ 100
#define DUI_R2D_AUTHORIZATION_TIMEOUT_MS 200

typedef struct {
    uint8_t r2d_authorized;
    // Could not determine type for signal: temp_imd_1
    // Could not determine type for signal: temp_imd_2
    uint8_t dui_imd_faults;
    // Could not determine type for signal: temp_ams_1
    // Could not determine type for signal: temp_ams_2
    uint8_t dui_ams_faults;
} msg_dui_r2d_authorization_t;

// Signal: R2D Authorized
#define DUI_R2D_AUTHORIZATION_R2D_AUTHORIZED_PREC 1.0f



// Bitfield Indices for: DUI IMD Faults
#define DUI_R2D_AUTHORIZATION_DUI_IMD_FAULTS_TEMP_IMD_1_IDX 0
#define DUI_R2D_AUTHORIZATION_DUI_IMD_FAULTS_TEMP_IMD_2_IDX 1



// Bitfield Indices for: DUI AMS Faults
#define DUI_R2D_AUTHORIZATION_DUI_AMS_FAULTS_TEMP_AMS_1_IDX 0
#define DUI_R2D_AUTHORIZATION_DUI_AMS_FAULTS_TEMP_AMS_2_IDX 1

int pack_dui_r2d_authorization(const msg_dui_r2d_authorization_t* msg, uint8_t* tx_buf);
int unpack_dui_r2d_authorization(const uint8_t* rx_buf, msg_dui_r2d_authorization_t* msg);

// ==========================================================================
// Packet: Wheel Speed, Ride height (304)
// ==========================================================================
// From: USM
// To:   VCU
#define WHEEL_SPEED_RIDE_HEIGHT_ID 304
#define WHEEL_SPEED_RIDE_HEIGHT_DLC 4
#define WHEEL_SPEED_RIDE_HEIGHT_FREQ 100
#define WHEEL_SPEED_RIDE_HEIGHT_TIMEOUT_MS 200

typedef struct {
    float wheel_speed;
    float ride_height;
} msg_wheel_speed_ride_height_t;

// Signal: Wheel Speed
#define WHEEL_SPEED_RIDE_HEIGHT_WHEEL_SPEED_PREC 0.0078125f

// Signal: Ride Height
#define WHEEL_SPEED_RIDE_HEIGHT_RIDE_HEIGHT_PREC 0.0625f

int pack_wheel_speed_ride_height(const msg_wheel_speed_ride_height_t* msg, uint8_t* tx_buf);
int unpack_wheel_speed_ride_height(const uint8_t* rx_buf, msg_wheel_speed_ride_height_t* msg);

// ==========================================================================
// Packet: Contactor Status (305)
// ==========================================================================
// From: HVC
// To:   VCU
#define CONTACTOR_STATUS_ID 305
#define CONTACTOR_STATUS_DLC 3
#define CONTACTOR_STATUS_FREQ 10
#define CONTACTOR_STATUS_TIMEOUT_MS 20

typedef struct {
    uint8_t hvc_state_machine;
    uint8_t positive_hv_contactor;
    uint8_t negative_hv_contactor;
    uint8_t precharge_contactor;
} msg_contactor_status_t;

// Signal: HVC State Machine
#define CONTACTOR_STATUS_HVC_STATE_MACHINE_PREC 1.0f

// Signal: Positive HV Contactor
#define CONTACTOR_STATUS_POSITIVE_HV_CONTACTOR_PREC 1.0f

// Signal: Negative HV Contactor
#define CONTACTOR_STATUS_NEGATIVE_HV_CONTACTOR_PREC 1.0f

// Signal: Precharge Contactor
#define CONTACTOR_STATUS_PRECHARGE_CONTACTOR_PREC 1.0f

int pack_contactor_status(const msg_contactor_status_t* msg, uint8_t* tx_buf);
int unpack_contactor_status(const uint8_t* rx_buf, msg_contactor_status_t* msg);

// ==========================================================================
// Packet: Battery Pack Status (306)
// ==========================================================================
// From: HVC
// To:   PDU
#define BATTERY_PACK_STATUS_ID 306
#define BATTERY_PACK_STATUS_DLC 8
#define BATTERY_PACK_STATUS_FREQ 100
#define BATTERY_PACK_STATUS_TIMEOUT_MS 200

typedef struct {
    float pack_voltage;
    float tractive_current;
    float state_of_charge;
    uint8_t cell_top_temp;
    uint8_t cell_bottom_temp;
} msg_battery_pack_status_t;

// Signal: Pack Voltage
#define BATTERY_PACK_STATUS_PACK_VOLTAGE_PREC 0.01f

// Signal: Tractive Current
#define BATTERY_PACK_STATUS_TRACTIVE_CURRENT_PREC 0.01f

// Signal: State of Charge
#define BATTERY_PACK_STATUS_STATE_OF_CHARGE_PREC 0.01f

// Signal: Cell Top Temp
#define BATTERY_PACK_STATUS_CELL_TOP_TEMP_PREC 1.0f

// Signal: Cell Bottom Temp
#define BATTERY_PACK_STATUS_CELL_BOTTOM_TEMP_PREC 1.0f

int pack_battery_pack_status(const msg_battery_pack_status_t* msg, uint8_t* tx_buf);
int unpack_battery_pack_status(const uint8_t* rx_buf, msg_battery_pack_status_t* msg);

// ==========================================================================
// Packet: Battery Temperature Status (307)
// ==========================================================================
// From: HVC
// To:   PDU
#define BATTERY_TEMPERATURE_STATUS_ID 307
#define BATTERY_TEMPERATURE_STATUS_DLC 8
#define BATTERY_TEMPERATURE_STATUS_FREQ 100
#define BATTERY_TEMPERATURE_STATUS_TIMEOUT_MS 200

typedef struct {
    float bus_bar_1_temp;
    float bus_bar_2_temp;
    float bus_bar_3_temp;
    float precharge_resistor_temp;
} msg_battery_temperature_status_t;

// Signal: Bus Bar 1 Temp
#define BATTERY_TEMPERATURE_STATUS_BUS_BAR_1_TEMP_PREC 0.1f

// Signal: Bus Bar 2 Temp
#define BATTERY_TEMPERATURE_STATUS_BUS_BAR_2_TEMP_PREC 0.1f

// Signal: Bus Bar 3 Temp
#define BATTERY_TEMPERATURE_STATUS_BUS_BAR_3_TEMP_PREC 0.1f

// Signal: Precharge Resistor Temp
#define BATTERY_TEMPERATURE_STATUS_PRECHARGE_RESISTOR_TEMP_PREC 0.1f

int pack_battery_temperature_status(const msg_battery_temperature_status_t* msg, uint8_t* tx_buf);
int unpack_battery_temperature_status(const uint8_t* rx_buf, msg_battery_temperature_status_t* msg);

// ==========================================================================
// Packet: Indicators + Shutdown Status (308)
// ==========================================================================
// From: HVC
// To:   PDU
#define INDICATORS_SHUTDOWN_STATUS_ID 308
#define INDICATORS_SHUTDOWN_STATUS_DLC 6
#define INDICATORS_SHUTDOWN_STATUS_FREQ 100
#define INDICATORS_SHUTDOWN_STATUS_TIMEOUT_MS 200

typedef struct {
    uint8_t bms_error;
    uint8_t imd_error;
    uint8_t shutdown_leg_1;
    uint8_t shutdown_leg_2;
    uint8_t shutdown_leg_3;
    uint8_t shutdown_leg_4;
} msg_indicators_shutdown_status_t;

// Signal: BMS Error
#define INDICATORS_SHUTDOWN_STATUS_BMS_ERROR_PREC 1.0f

// Signal: IMD Error
#define INDICATORS_SHUTDOWN_STATUS_IMD_ERROR_PREC 1.0f

// Signal: Shutdown Leg 1
#define INDICATORS_SHUTDOWN_STATUS_SHUTDOWN_LEG_1_PREC 1.0f

// Signal: Shutdown Leg 2
#define INDICATORS_SHUTDOWN_STATUS_SHUTDOWN_LEG_2_PREC 1.0f

// Signal: Shutdown Leg 3
#define INDICATORS_SHUTDOWN_STATUS_SHUTDOWN_LEG_3_PREC 1.0f

// Signal: Shutdown Leg 4
#define INDICATORS_SHUTDOWN_STATUS_SHUTDOWN_LEG_4_PREC 1.0f

int pack_indicators_shutdown_status(const msg_indicators_shutdown_status_t* msg, uint8_t* tx_buf);
int unpack_indicators_shutdown_status(const uint8_t* rx_buf, msg_indicators_shutdown_status_t* msg);

// ==========================================================================
// Packet: Allow Balance Command (309)
// ==========================================================================
// From: VCU
// To:   HVC
#define ALLOW_BALANCE_COMMAND_ID 309
#define ALLOW_BALANCE_COMMAND_DLC 0
#define ALLOW_BALANCE_COMMAND_FREQ 0
#define ALLOW_BALANCE_COMMAND_TIMEOUT_MS 0

typedef struct {
} msg_allow_balance_command_t;

int pack_allow_balance_command(const msg_allow_balance_command_t* msg, uint8_t* tx_buf);
int unpack_allow_balance_command(const uint8_t* rx_buf, msg_allow_balance_command_t* msg);

// ==========================================================================
// Packet: Battery Cell Limits (310)
// ==========================================================================
// From: HVC
// To:   VCU
#define BATTERY_CELL_LIMITS_ID 310
#define BATTERY_CELL_LIMITS_DLC 4
#define BATTERY_CELL_LIMITS_FREQ 100
#define BATTERY_CELL_LIMITS_TIMEOUT_MS 200

typedef struct {
    float min_cell_voltage;
    float max_cell_voltage;
} msg_battery_cell_limits_t;

// Signal: Min Cell Voltage
#define BATTERY_CELL_LIMITS_MIN_CELL_VOLTAGE_PREC 0.0001f

// Signal: Max Cell Voltage
#define BATTERY_CELL_LIMITS_MAX_CELL_VOLTAGE_PREC 0.0001f

int pack_battery_cell_limits(const msg_battery_cell_limits_t* msg, uint8_t* tx_buf);
int unpack_battery_cell_limits(const uint8_t* rx_buf, msg_battery_cell_limits_t* msg);

// ==========================================================================
// Packet: VCU Shutdown Status (320)
// ==========================================================================
// From: VCU
// To:   PDU
#define VCU_SHUTDOWN_STATUS_ID 320
#define VCU_SHUTDOWN_STATUS_DLC 1
#define VCU_SHUTDOWN_STATUS_FREQ 3
#define VCU_SHUTDOWN_STATUS_TIMEOUT_MS 6

typedef struct {
    // Could not determine type for signal: shutdown_bspd_status
    // Could not determine type for signal: shutdown_emeter_status
    uint8_t vcu_shutdown_status;
} msg_vcu_shutdown_status_t;



// Bitfield Indices for: VCU Shutdown Status
#define VCU_SHUTDOWN_STATUS_VCU_SHUTDOWN_STATUS_SHUTDOWN_BSPD_STATUS_IDX 0
#define VCU_SHUTDOWN_STATUS_VCU_SHUTDOWN_STATUS_SHUTDOWN_EMETER_STATUS_IDX 1

int pack_vcu_shutdown_status(const msg_vcu_shutdown_status_t* msg, uint8_t* tx_buf);
int unpack_vcu_shutdown_status(const uint8_t* rx_buf, msg_vcu_shutdown_status_t* msg);

// ==========================================================================
// Packet: VCU Fuses (321)
// ==========================================================================
// From: VCU
// To:   PDU
#define VCU_FUSES_ID 321
#define VCU_FUSES_DLC 2
#define VCU_FUSES_FREQ 3
#define VCU_FUSES_TIMEOUT_MS 6

typedef struct {
    // Could not determine type for signal: batt_pump_fuse
    // Could not determine type for signal: tssi_green_fuse
    // Could not determine type for signal: tssi_red_fuse
    // Could not determine type for signal: batt_fans_fuse
    // Could not determine type for signal: shtdn_fuse
    // Could not determine type for signal: ll_fuse
    // Could not determine type for signal: motor_pump_fuse
    // Could not determine type for signal: boards_fuse
    uint8_t vcu_fuses_1;
    // Could not determine type for signal: brake_light_fuse
    // Could not determine type for signal: rtd_fuse
    // Could not determine type for signal: spare_fuse
    uint8_t vcu_fuses_2;
} msg_vcu_fuses_t;









// Bitfield Indices for: VCU Fuses 1
#define VCU_FUSES_VCU_FUSES_1_BATT_PUMP_FUSE_IDX 0
#define VCU_FUSES_VCU_FUSES_1_TSSI_GREEN_FUSE_IDX 1
#define VCU_FUSES_VCU_FUSES_1_TSSI_RED_FUSE_IDX 2
#define VCU_FUSES_VCU_FUSES_1_BATT_FANS_FUSE_IDX 3
#define VCU_FUSES_VCU_FUSES_1_SHTDN_FUSE_IDX 4
#define VCU_FUSES_VCU_FUSES_1_LL_FUSE_IDX 5
#define VCU_FUSES_VCU_FUSES_1_MOTOR_PUMP_FUSE_IDX 6
#define VCU_FUSES_VCU_FUSES_1_BOARDS_FUSE_IDX 7




// Bitfield Indices for: VCU Fuses 2
#define VCU_FUSES_VCU_FUSES_2_BRAKE_LIGHT_FUSE_IDX 0
#define VCU_FUSES_VCU_FUSES_2_RTD_FUSE_IDX 1
#define VCU_FUSES_VCU_FUSES_2_SPARE_FUSE_IDX 2

int pack_vcu_fuses(const msg_vcu_fuses_t* msg, uint8_t* tx_buf);
int unpack_vcu_fuses(const uint8_t* rx_buf, msg_vcu_fuses_t* msg);

// ==========================================================================
// Packet: VCU Current Sense (322)
// ==========================================================================
// From: VCU
// To:   PDU
#define VCU_CURRENT_SENSE_ID 322
#define VCU_CURRENT_SENSE_DLC 5
#define VCU_CURRENT_SENSE_FREQ 3
#define VCU_CURRENT_SENSE_TIMEOUT_MS 6

typedef struct {
    float lv_boards_current;
    float shutdown_current;
    float battery_cooling_current;
    float motor_cooling_current;
    float lights_current_current;
} msg_vcu_current_sense_t;

// Signal: LV Boards Current
#define VCU_CURRENT_SENSE_LV_BOARDS_CURRENT_PREC 0.04f

// Signal: Shutdown Current
#define VCU_CURRENT_SENSE_SHUTDOWN_CURRENT_PREC 0.04f

// Signal: Battery Cooling Current
#define VCU_CURRENT_SENSE_BATTERY_COOLING_CURRENT_PREC 0.04f

// Signal: Motor Cooling Current
#define VCU_CURRENT_SENSE_MOTOR_COOLING_CURRENT_PREC 0.04f

// Signal: Lights Current Current
#define VCU_CURRENT_SENSE_LIGHTS_CURRENT_CURRENT_PREC 0.04f

int pack_vcu_current_sense(const msg_vcu_current_sense_t* msg, uint8_t* tx_buf);
int unpack_vcu_current_sense(const uint8_t* rx_buf, msg_vcu_current_sense_t* msg);

// ==========================================================================
// Packet: Switch Command (323)
// ==========================================================================
// From: VCU
// To:   PDU
#define SWITCH_COMMAND_ID 323
#define SWITCH_COMMAND_DLC 2
#define SWITCH_COMMAND_FREQ 100
#define SWITCH_COMMAND_TIMEOUT_MS 200

typedef struct {
    // Could not determine type for signal: temp_command_1
    // Could not determine type for signal: temp_command_2
    uint8_t switch_command;
} msg_switch_command_t;



// Bitfield Indices for: Switch Command
#define SWITCH_COMMAND_SWITCH_COMMAND_TEMP_COMMAND_1_IDX 0
#define SWITCH_COMMAND_SWITCH_COMMAND_TEMP_COMMAND_2_IDX 1

int pack_switch_command(const msg_switch_command_t* msg, uint8_t* tx_buf);
int unpack_switch_command(const uint8_t* rx_buf, msg_switch_command_t* msg);

// ==========================================================================
// Packet: Switch Outputs (324)
// ==========================================================================
// From: PDU
// To:   VCU
#define SWITCH_OUTPUTS_ID 324
#define SWITCH_OUTPUTS_DLC 2
#define SWITCH_OUTPUTS_FREQ 100
#define SWITCH_OUTPUTS_TIMEOUT_MS 200

typedef struct {
    // Could not determine type for signal: temp_output_1
    // Could not determine type for signal: temp_output_2
    uint8_t switch_output;
} msg_switch_outputs_t;



// Bitfield Indices for: Switch Output
#define SWITCH_OUTPUTS_SWITCH_OUTPUT_TEMP_OUTPUT_1_IDX 0
#define SWITCH_OUTPUTS_SWITCH_OUTPUT_TEMP_OUTPUT_2_IDX 1

int pack_switch_outputs(const msg_switch_outputs_t* msg, uint8_t* tx_buf);
int unpack_switch_outputs(const uint8_t* rx_buf, msg_switch_outputs_t* msg);

// ==========================================================================
// Packet: Battery Cooling (385)
// ==========================================================================
// From: PDU
// To:   Pi
#define BATTERY_COOLING_ID 385
#define BATTERY_COOLING_DLC 6
#define BATTERY_COOLING_FREQ 100
#define BATTERY_COOLING_TIMEOUT_MS 200

typedef struct {
    float temp_after_battery;
    float temp_after_radiator;
    float radiator_fan_speed;
} msg_battery_cooling_t;

// Signal: Temp After Battery
#define BATTERY_COOLING_TEMP_AFTER_BATTERY_PREC 0.01f

// Signal: Temp After Radiator
#define BATTERY_COOLING_TEMP_AFTER_RADIATOR_PREC 0.01f

// Signal: Radiator Fan Speed
#define BATTERY_COOLING_RADIATOR_FAN_SPEED_PREC 0.2f

int pack_battery_cooling(const msg_battery_cooling_t* msg, uint8_t* tx_buf);
int unpack_battery_cooling(const uint8_t* rx_buf, msg_battery_cooling_t* msg);

// ==========================================================================
// Packet: Temps (386)
// ==========================================================================
// From: PDU
// To:   Pi
#define TEMPS_ID 386
#define TEMPS_DLC 8
#define TEMPS_FREQ 100
#define TEMPS_TIMEOUT_MS 200

typedef struct {
    float inverter;
    float motor;
    float ambient;
    float discharge_resistor_temp;
} msg_temps_t;

// Signal: Inverter
#define TEMPS_INVERTER_PREC 0.01f

// Signal: Motor
#define TEMPS_MOTOR_PREC 0.01f

// Signal: Ambient
#define TEMPS_AMBIENT_PREC 0.01f

// Signal: Discharge Resistor Temp
#define TEMPS_DISCHARGE_RESISTOR_TEMP_PREC 0.01f

int pack_temps(const msg_temps_t* msg, uint8_t* tx_buf);
int unpack_temps(const uint8_t* rx_buf, msg_temps_t* msg);

// ==========================================================================
// Packet: LV Battery (387)
// ==========================================================================
// From: PDU
// To:   PI
#define LV_BATTERY_ID 387
#define LV_BATTERY_DLC 6
#define LV_BATTERY_FREQ 100
#define LV_BATTERY_TIMEOUT_MS 200

typedef struct {
    float lv_battery_voltage;
    float lv_battery_current;
    float lv_battery_temp;
} msg_lv_battery_t;

// Signal: LV Battery Voltage
#define LV_BATTERY_LV_BATTERY_VOLTAGE_PREC 0.01f

// Signal: LV Battery Current
#define LV_BATTERY_LV_BATTERY_CURRENT_PREC 0.01f

// Signal: LV Battery Temp
#define LV_BATTERY_LV_BATTERY_TEMP_PREC 0.01f

int pack_lv_battery(const msg_lv_battery_t* msg, uint8_t* tx_buf);
int unpack_lv_battery(const uint8_t* rx_buf, msg_lv_battery_t* msg);

// ==========================================================================
// Packet: Coolant Loop Temps (416)
// ==========================================================================
// From: TSM
// To:   Pi
#define COOLANT_LOOP_TEMPS_ID 416
#define COOLANT_LOOP_TEMPS_DLC 8
#define COOLANT_LOOP_TEMPS_FREQ 100
#define COOLANT_LOOP_TEMPS_TIMEOUT_MS 200

typedef struct {
    float loop_temp_after_motor;
    float loop_temp_after_inverter;
    float temp_after_radiator;
} msg_coolant_loop_temps_t;

// Signal: Loop Temp After Motor
#define COOLANT_LOOP_TEMPS_LOOP_TEMP_AFTER_MOTOR_PREC 0.01f

// Signal: Loop Temp After Inverter
#define COOLANT_LOOP_TEMPS_LOOP_TEMP_AFTER_INVERTER_PREC 0.01f

// Signal: Temp After Radiator
#define COOLANT_LOOP_TEMPS_TEMP_AFTER_RADIATOR_PREC 0.01f

int pack_coolant_loop_temps(const msg_coolant_loop_temps_t* msg, uint8_t* tx_buf);
int unpack_coolant_loop_temps(const uint8_t* rx_buf, msg_coolant_loop_temps_t* msg);

// ==========================================================================
// Packet: Fan/Flow Speeds (418)
// ==========================================================================
// From: TSM
// To:   Pi
#define FAN_FLOW_SPEEDS_ID 418
#define FAN_FLOW_SPEEDS_DLC 8
#define FAN_FLOW_SPEEDS_FREQ 100
#define FAN_FLOW_SPEEDS_TIMEOUT_MS 200

typedef struct {
    uint16_t radiator_fan_speed;
    uint16_t battery_fan_speed;
    float coolant_flow;
    float ambient_temp;
} msg_fan_flow_speeds_t;

// Signal: Radiator Fan Speed
#define FAN_FLOW_SPEEDS_RADIATOR_FAN_SPEED_PREC 1.0f

// Signal: Battery Fan Speed
#define FAN_FLOW_SPEEDS_BATTERY_FAN_SPEED_PREC 1.0f

// Signal: Coolant Flow
#define FAN_FLOW_SPEEDS_COOLANT_FLOW_PREC 0.01f

// Signal: Ambient Temp
#define FAN_FLOW_SPEEDS_AMBIENT_TEMP_PREC 0.01f

int pack_fan_flow_speeds(const msg_fan_flow_speeds_t* msg, uint8_t* tx_buf);
int unpack_fan_flow_speeds(const uint8_t* rx_buf, msg_fan_flow_speeds_t* msg);

// ==========================================================================
// Packet: APPS Voltages (448)
// ==========================================================================
// From: VCU
// To:   PI
#define APPS_VOLTAGES_ID 448
#define APPS_VOLTAGES_DLC 8
#define APPS_VOLTAGES_FREQ 3
#define APPS_VOLTAGES_TIMEOUT_MS 6

typedef struct {
    float apps1_voltage;
    float apps2_voltage;
    float apps1_travel;
    float apps2_travel;
} msg_apps_voltages_t;

// Signal: APPS1 Voltage
#define APPS_VOLTAGES_APPS1_VOLTAGE_PREC 0.0001f

// Signal: APPS2 Voltage
#define APPS_VOLTAGES_APPS2_VOLTAGE_PREC 0.0001f

// Signal: APPS1 Travel
#define APPS_VOLTAGES_APPS1_TRAVEL_PREC 0.0001f

// Signal: APPS2 Travel
#define APPS_VOLTAGES_APPS2_TRAVEL_PREC 0.0001f

int pack_apps_voltages(const msg_apps_voltages_t* msg, uint8_t* tx_buf);
int unpack_apps_voltages(const uint8_t* rx_buf, msg_apps_voltages_t* msg);

// ==========================================================================
// Packet: Accelerator Pedal (449)
// ==========================================================================
// From: VCU
// To:   PI
#define ACCELERATOR_PEDAL_ID 449
#define ACCELERATOR_PEDAL_DLC 3
#define ACCELERATOR_PEDAL_FREQ 3
#define ACCELERATOR_PEDAL_TIMEOUT_MS 6

typedef struct {
    float accelerator_pedal_travel;
    // Could not determine type for signal: apps1_disconnect
    // Could not determine type for signal: apps2_disconnect
    // Could not determine type for signal: apps1_out_range
    // Could not determine type for signal: apps2_out_range
    // Could not determine type for signal: apps_mismatch
    // Could not determine type for signal: apps_implause
    uint8_t apps_faults;
} msg_accelerator_pedal_t;

// Signal: Accelerator Pedal Travel
#define ACCELERATOR_PEDAL_ACCELERATOR_PEDAL_TRAVEL_PREC 0.0001f







// Bitfield Indices for: APPS Faults
#define ACCELERATOR_PEDAL_APPS_FAULTS_APPS1_DISCONNECT_IDX 0
#define ACCELERATOR_PEDAL_APPS_FAULTS_APPS2_DISCONNECT_IDX 1
#define ACCELERATOR_PEDAL_APPS_FAULTS_APPS1_OUT_RANGE_IDX 2
#define ACCELERATOR_PEDAL_APPS_FAULTS_APPS2_OUT_RANGE_IDX 3
#define ACCELERATOR_PEDAL_APPS_FAULTS_APPS_MISMATCH_IDX 4
#define ACCELERATOR_PEDAL_APPS_FAULTS_APPS_IMPLAUSE_IDX 5

int pack_accelerator_pedal(const msg_accelerator_pedal_t* msg, uint8_t* tx_buf);
int unpack_accelerator_pedal(const uint8_t* rx_buf, msg_accelerator_pedal_t* msg);

// ==========================================================================
// Packet: BPPS Voltages (450)
// ==========================================================================
// From: VCU
// To:   PI
#define BPPS_VOLTAGES_ID 450
#define BPPS_VOLTAGES_DLC 8
#define BPPS_VOLTAGES_FREQ 3
#define BPPS_VOLTAGES_TIMEOUT_MS 6

typedef struct {
    float bpps1_voltage;
    float bpps2_voltage;
    float bpps1_travel;
    float bpps2_travel;
} msg_bpps_voltages_t;

// Signal: BPPS1 Voltage
#define BPPS_VOLTAGES_BPPS1_VOLTAGE_PREC 0.0001f

// Signal: BPPS2 Voltage
#define BPPS_VOLTAGES_BPPS2_VOLTAGE_PREC 0.0001f

// Signal: BPPS1 Travel
#define BPPS_VOLTAGES_BPPS1_TRAVEL_PREC 0.0001f

// Signal: BPPS2 Travel
#define BPPS_VOLTAGES_BPPS2_TRAVEL_PREC 0.0001f

int pack_bpps_voltages(const msg_bpps_voltages_t* msg, uint8_t* tx_buf);
int unpack_bpps_voltages(const uint8_t* rx_buf, msg_bpps_voltages_t* msg);

// ==========================================================================
// Packet: Brake Pedal (451)
// ==========================================================================
// From: VCU
// To:   PI
#define BRAKE_PEDAL_ID 451
#define BRAKE_PEDAL_DLC 5
#define BRAKE_PEDAL_FREQ 3
#define BRAKE_PEDAL_TIMEOUT_MS 6

typedef struct {
    float brake_pedal_travel;
    // Could not determine type for signal: bpps1_disconnect
    // Could not determine type for signal: bpps2_disconnect
    // Could not determine type for signal: bpps1_out_range
    // Could not determine type for signal: bpps2_out_range
    // Could not determine type for signal: bpps_mismatch
    uint8_t bpps_faults;
    float brake_light_percent;
} msg_brake_pedal_t;

// Signal: Brake Pedal Travel
#define BRAKE_PEDAL_BRAKE_PEDAL_TRAVEL_PREC 0.0001f






// Bitfield Indices for: BPPS Faults
#define BRAKE_PEDAL_BPPS_FAULTS_BPPS1_DISCONNECT_IDX 0
#define BRAKE_PEDAL_BPPS_FAULTS_BPPS2_DISCONNECT_IDX 1
#define BRAKE_PEDAL_BPPS_FAULTS_BPPS1_OUT_RANGE_IDX 2
#define BRAKE_PEDAL_BPPS_FAULTS_BPPS2_OUT_RANGE_IDX 3
#define BRAKE_PEDAL_BPPS_FAULTS_BPPS_MISMATCH_IDX 4

// Signal: Brake Light Percent
#define BRAKE_PEDAL_BRAKE_LIGHT_PERCENT_PREC 0.001f

int pack_brake_pedal(const msg_brake_pedal_t* msg, uint8_t* tx_buf);
int unpack_brake_pedal(const uint8_t* rx_buf, msg_brake_pedal_t* msg);

// ==========================================================================
// Packet: BSE Voltages (452)
// ==========================================================================
// From: VCU
// To:   Pi
#define BSE_VOLTAGES_ID 452
#define BSE_VOLTAGES_DLC 6
#define BSE_VOLTAGES_FREQ 3
#define BSE_VOLTAGES_TIMEOUT_MS 6

typedef struct {
    float bse_front_voltage;
    float bse_rear_voltage;
    float bse_line_lock_voltage;
} msg_bse_voltages_t;

// Signal: BSE Front Voltage
#define BSE_VOLTAGES_BSE_FRONT_VOLTAGE_PREC 0.0001f

// Signal: BSE Rear Voltage
#define BSE_VOLTAGES_BSE_REAR_VOLTAGE_PREC 0.0001f

// Signal: BSE Line Lock Voltage
#define BSE_VOLTAGES_BSE_LINE_LOCK_VOLTAGE_PREC 0.0001f

int pack_bse_voltages(const msg_bse_voltages_t* msg, uint8_t* tx_buf);
int unpack_bse_voltages(const uint8_t* rx_buf, msg_bse_voltages_t* msg);

// ==========================================================================
// Packet: Brakes (453)
// ==========================================================================
// From: VCU
// To:   Pi
#define BRAKES_ID 453
#define BRAKES_DLC 8
#define BRAKES_FREQ 3
#define BRAKES_TIMEOUT_MS 6

typedef struct {
    float brake_pressure_front;
    float brake_pressure_rear_pre_lock;
    float brake_pressure_rear_post_lock;
    float brake_bias;
    // Could not determine type for signal: bse1_disconnect
    // Could not determine type for signal: bse2_disconnect
    // Could not determine type for signal: bse1_out_range
    // Could not determine type for signal: bse2_out_range
    uint8_t bse_faults;
} msg_brakes_t;

// Signal: Brake Pressure Front
#define BRAKES_BRAKE_PRESSURE_FRONT_PREC 0.05f

// Signal: Brake Pressure Rear Pre Lock
#define BRAKES_BRAKE_PRESSURE_REAR_PRE_LOCK_PREC 0.05f

// Signal: Brake Pressure Rear Post Lock
#define BRAKES_BRAKE_PRESSURE_REAR_POST_LOCK_PREC 0.05f

// Signal: Brake Bias
#define BRAKES_BRAKE_BIAS_PREC 0.01f





// Bitfield Indices for: BSE Faults
#define BRAKES_BSE_FAULTS_BSE1_DISCONNECT_IDX 0
#define BRAKES_BSE_FAULTS_BSE2_DISCONNECT_IDX 1
#define BRAKES_BSE_FAULTS_BSE1_OUT_RANGE_IDX 2
#define BRAKES_BSE_FAULTS_BSE2_OUT_RANGE_IDX 3

int pack_brakes(const msg_brakes_t* msg, uint8_t* tx_buf);
int unpack_brakes(const uint8_t* rx_buf, msg_brakes_t* msg);

// ==========================================================================
// Packet: Steering Column (454)
// ==========================================================================
// From: VCU
// To:   Pi
#define STEERING_COLUMN_ID 454
#define STEERING_COLUMN_DLC 2
#define STEERING_COLUMN_FREQ 3
#define STEERING_COLUMN_TIMEOUT_MS 6

typedef struct {
    float steering_column_angle;
} msg_steering_column_t;

// Signal: Steering Column Angle
#define STEERING_COLUMN_STEERING_COLUMN_ANGLE_PREC 0.004f

int pack_steering_column(const msg_steering_column_t* msg, uint8_t* tx_buf);
int unpack_steering_column(const uint8_t* rx_buf, msg_steering_column_t* msg);

// ==========================================================================
// Packet: VCU State (455)
// ==========================================================================
// From: VCU
// To:   Pi
#define VCU_STATE_ID 455
#define VCU_STATE_DLC 6
#define VCU_STATE_FREQ 3
#define VCU_STATE_TIMEOUT_MS 6

typedef struct {
    uint8_t prndl_state;
    uint8_t stomp_fault;
    uint8_t ready_to_drive_buzzer;
    float state_of_charge_estimate;
    uint8_t line_lock_enabled;
} msg_vcu_state_t;

// Signal: PRNDL State
#define VCU_STATE_PRNDL_STATE_PREC 1.0f

// Signal: STOMP Fault
#define VCU_STATE_STOMP_FAULT_PREC 1.0f

// Signal: Ready To Drive Buzzer
#define VCU_STATE_READY_TO_DRIVE_BUZZER_PREC 1.0f

// Signal: State of Charge Estimate
#define VCU_STATE_STATE_OF_CHARGE_ESTIMATE_PREC 0.1f

// Signal: Line Lock Enabled
#define VCU_STATE_LINE_LOCK_ENABLED_PREC 1.0f

int pack_vcu_state(const msg_vcu_state_t* msg, uint8_t* tx_buf);
int unpack_vcu_state(const uint8_t* rx_buf, msg_vcu_state_t* msg);

// ==========================================================================
// Packet: Wheel Speeds (1024)
// ==========================================================================
// From: USM
// To:   Pi
#define WHEEL_SPEEDS_ID 1024
#define WHEEL_SPEEDS_DLC 8
#define WHEEL_SPEEDS_FREQ 10
#define WHEEL_SPEEDS_TIMEOUT_MS 20

typedef struct {
    float front_left_speed;
    float front_right_speed;
    float back_left_speed;
    float back_right_speed;
} msg_wheel_speeds_t;

// Signal: Front Left Speed
#define WHEEL_SPEEDS_FRONT_LEFT_SPEED_PREC 0.01f

// Signal: Front Right Speed
#define WHEEL_SPEEDS_FRONT_RIGHT_SPEED_PREC 0.01f

// Signal: Back Left Speed
#define WHEEL_SPEEDS_BACK_LEFT_SPEED_PREC 0.01f

// Signal: Back Right Speed
#define WHEEL_SPEEDS_BACK_RIGHT_SPEED_PREC 0.01f

int pack_wheel_speeds(const msg_wheel_speeds_t* msg, uint8_t* tx_buf);
int unpack_wheel_speeds(const uint8_t* rx_buf, msg_wheel_speeds_t* msg);

// ==========================================================================
// Packet: Acceleration Vector Unsprung FL (1026)
// ==========================================================================
// From: USM
// To:   Pi
#define ACCELERATION_VECTOR_UNSPRUNG_FL_ID 1026
#define ACCELERATION_VECTOR_UNSPRUNG_FL_DLC 6
#define ACCELERATION_VECTOR_UNSPRUNG_FL_FREQ 10
#define ACCELERATION_VECTOR_UNSPRUNG_FL_TIMEOUT_MS 20

typedef struct {
    float x;
    float y;
    float z;
} msg_acceleration_vector_unsprung_fl_t;

// Signal: X
#define ACCELERATION_VECTOR_UNSPRUNG_FL_X_PREC 0.001f

// Signal: Y
#define ACCELERATION_VECTOR_UNSPRUNG_FL_Y_PREC 0.001f

// Signal: Z
#define ACCELERATION_VECTOR_UNSPRUNG_FL_Z_PREC 0.001f

int pack_acceleration_vector_unsprung_fl(const msg_acceleration_vector_unsprung_fl_t* msg, uint8_t* tx_buf);
int unpack_acceleration_vector_unsprung_fl(const uint8_t* rx_buf, msg_acceleration_vector_unsprung_fl_t* msg);

// ==========================================================================
// Packet: Acceleration Vector Unsprung FR (1027)
// ==========================================================================
// From: USM
// To:   Pi
#define ACCELERATION_VECTOR_UNSPRUNG_FR_ID 1027
#define ACCELERATION_VECTOR_UNSPRUNG_FR_DLC 6
#define ACCELERATION_VECTOR_UNSPRUNG_FR_FREQ 10
#define ACCELERATION_VECTOR_UNSPRUNG_FR_TIMEOUT_MS 20

typedef struct {
    float x;
    float y;
    float z;
} msg_acceleration_vector_unsprung_fr_t;

// Signal: X
#define ACCELERATION_VECTOR_UNSPRUNG_FR_X_PREC 0.001f

// Signal: Y
#define ACCELERATION_VECTOR_UNSPRUNG_FR_Y_PREC 0.001f

// Signal: Z
#define ACCELERATION_VECTOR_UNSPRUNG_FR_Z_PREC 0.001f

int pack_acceleration_vector_unsprung_fr(const msg_acceleration_vector_unsprung_fr_t* msg, uint8_t* tx_buf);
int unpack_acceleration_vector_unsprung_fr(const uint8_t* rx_buf, msg_acceleration_vector_unsprung_fr_t* msg);

// ==========================================================================
// Packet: Acceleration Vector Unsprung RL (1028)
// ==========================================================================
// From: USM
// To:   Pi
#define ACCELERATION_VECTOR_UNSPRUNG_RL_ID 1028
#define ACCELERATION_VECTOR_UNSPRUNG_RL_DLC 6
#define ACCELERATION_VECTOR_UNSPRUNG_RL_FREQ 10
#define ACCELERATION_VECTOR_UNSPRUNG_RL_TIMEOUT_MS 20

typedef struct {
    float x;
    float y;
    float z;
} msg_acceleration_vector_unsprung_rl_t;

// Signal: X
#define ACCELERATION_VECTOR_UNSPRUNG_RL_X_PREC 0.001f

// Signal: Y
#define ACCELERATION_VECTOR_UNSPRUNG_RL_Y_PREC 0.001f

// Signal: Z
#define ACCELERATION_VECTOR_UNSPRUNG_RL_Z_PREC 0.001f

int pack_acceleration_vector_unsprung_rl(const msg_acceleration_vector_unsprung_rl_t* msg, uint8_t* tx_buf);
int unpack_acceleration_vector_unsprung_rl(const uint8_t* rx_buf, msg_acceleration_vector_unsprung_rl_t* msg);

// ==========================================================================
// Packet: Acceleration Vector Unsprung RR (1029)
// ==========================================================================
// From: USM
// To:   Pi
#define ACCELERATION_VECTOR_UNSPRUNG_RR_ID 1029
#define ACCELERATION_VECTOR_UNSPRUNG_RR_DLC 6
#define ACCELERATION_VECTOR_UNSPRUNG_RR_FREQ 10
#define ACCELERATION_VECTOR_UNSPRUNG_RR_TIMEOUT_MS 20

typedef struct {
    float x;
    float y;
    float z;
} msg_acceleration_vector_unsprung_rr_t;

// Signal: X
#define ACCELERATION_VECTOR_UNSPRUNG_RR_X_PREC 0.001f

// Signal: Y
#define ACCELERATION_VECTOR_UNSPRUNG_RR_Y_PREC 0.001f

// Signal: Z
#define ACCELERATION_VECTOR_UNSPRUNG_RR_Z_PREC 0.001f

int pack_acceleration_vector_unsprung_rr(const msg_acceleration_vector_unsprung_rr_t* msg, uint8_t* tx_buf);
int unpack_acceleration_vector_unsprung_rr(const uint8_t* rx_buf, msg_acceleration_vector_unsprung_rr_t* msg);

// ==========================================================================
// Packet: FL Accel + Ride Height (1280)
// ==========================================================================
// From: CSM
// To:   Pi
#define FL_ACCEL_RIDE_HEIGHT_ID 1280
#define FL_ACCEL_RIDE_HEIGHT_DLC 8
#define FL_ACCEL_RIDE_HEIGHT_FREQ 10
#define FL_ACCEL_RIDE_HEIGHT_TIMEOUT_MS 20

typedef struct {
    float x;
    float y;
    float z;
    float ride_height;
} msg_fl_accel_ride_height_t;

// Signal: X
#define FL_ACCEL_RIDE_HEIGHT_X_PREC 0.001f

// Signal: Y
#define FL_ACCEL_RIDE_HEIGHT_Y_PREC 0.001f

// Signal: Z
#define FL_ACCEL_RIDE_HEIGHT_Z_PREC 0.001f

// Signal: Ride Height
#define FL_ACCEL_RIDE_HEIGHT_RIDE_HEIGHT_PREC 0.002f

int pack_fl_accel_ride_height(const msg_fl_accel_ride_height_t* msg, uint8_t* tx_buf);
int unpack_fl_accel_ride_height(const uint8_t* rx_buf, msg_fl_accel_ride_height_t* msg);

// ==========================================================================
// Packet: FL Strain Gauge + Sus Pot. (1281)
// ==========================================================================
// From: CSM
// To:   Pi
#define FL_STRAIN_GAUGE_SUS_POT_ID 1281
#define FL_STRAIN_GAUGE_SUS_POT_DLC 4
#define FL_STRAIN_GAUGE_SUS_POT_FREQ 10
#define FL_STRAIN_GAUGE_SUS_POT_TIMEOUT_MS 20

typedef struct {
    float front_left_strain_gauge_voltage;
    float front_left_suspension_potentiometer;
} msg_fl_strain_gauge_sus_pot_t;

// Signal: Front Left Strain Gauge Voltage
#define FL_STRAIN_GAUGE_SUS_POT_FRONT_LEFT_STRAIN_GAUGE_VOLTAGE_PREC 0.0002f

// Signal: Front Left Suspension Potentiometer
#define FL_STRAIN_GAUGE_SUS_POT_FRONT_LEFT_SUSPENSION_POTENTIOMETER_PREC 0.001f

int pack_fl_strain_gauge_sus_pot(const msg_fl_strain_gauge_sus_pot_t* msg, uint8_t* tx_buf);
int unpack_fl_strain_gauge_sus_pot(const uint8_t* rx_buf, msg_fl_strain_gauge_sus_pot_t* msg);

// ==========================================================================
// Packet: FR Accel + Ride Height (1282)
// ==========================================================================
// From: CSM
// To:   Pi
#define FR_ACCEL_RIDE_HEIGHT_ID 1282
#define FR_ACCEL_RIDE_HEIGHT_DLC 8
#define FR_ACCEL_RIDE_HEIGHT_FREQ 10
#define FR_ACCEL_RIDE_HEIGHT_TIMEOUT_MS 20

typedef struct {
    float x;
    float y;
    float z;
    float ride_height;
} msg_fr_accel_ride_height_t;

// Signal: X
#define FR_ACCEL_RIDE_HEIGHT_X_PREC 0.001f

// Signal: Y
#define FR_ACCEL_RIDE_HEIGHT_Y_PREC 0.001f

// Signal: Z
#define FR_ACCEL_RIDE_HEIGHT_Z_PREC 0.001f

// Signal: Ride Height
#define FR_ACCEL_RIDE_HEIGHT_RIDE_HEIGHT_PREC 0.002f

int pack_fr_accel_ride_height(const msg_fr_accel_ride_height_t* msg, uint8_t* tx_buf);
int unpack_fr_accel_ride_height(const uint8_t* rx_buf, msg_fr_accel_ride_height_t* msg);

// ==========================================================================
// Packet: FR Strain Gauge + Sus Pot. (1283)
// ==========================================================================
// From: CSM
// To:   Pi
#define FR_STRAIN_GAUGE_SUS_POT_ID 1283
#define FR_STRAIN_GAUGE_SUS_POT_DLC 4
#define FR_STRAIN_GAUGE_SUS_POT_FREQ 10
#define FR_STRAIN_GAUGE_SUS_POT_TIMEOUT_MS 20

typedef struct {
    float front_right_strain_gauge_voltage;
    float front_right_suspension_potentiometer;
} msg_fr_strain_gauge_sus_pot_t;

// Signal: Front Right Strain Gauge Voltage
#define FR_STRAIN_GAUGE_SUS_POT_FRONT_RIGHT_STRAIN_GAUGE_VOLTAGE_PREC 0.0002f

// Signal: Front Right Suspension Potentiometer
#define FR_STRAIN_GAUGE_SUS_POT_FRONT_RIGHT_SUSPENSION_POTENTIOMETER_PREC 0.001f

int pack_fr_strain_gauge_sus_pot(const msg_fr_strain_gauge_sus_pot_t* msg, uint8_t* tx_buf);
int unpack_fr_strain_gauge_sus_pot(const uint8_t* rx_buf, msg_fr_strain_gauge_sus_pot_t* msg);

// ==========================================================================
// Packet: RL Accel + Ride Height (1284)
// ==========================================================================
// From: CSM
// To:   Pi
#define RL_ACCEL_RIDE_HEIGHT_ID 1284
#define RL_ACCEL_RIDE_HEIGHT_DLC 8
#define RL_ACCEL_RIDE_HEIGHT_FREQ 10
#define RL_ACCEL_RIDE_HEIGHT_TIMEOUT_MS 20

typedef struct {
    float x;
    float y;
    float z;
    float ride_height;
} msg_rl_accel_ride_height_t;

// Signal: X
#define RL_ACCEL_RIDE_HEIGHT_X_PREC 0.001f

// Signal: Y
#define RL_ACCEL_RIDE_HEIGHT_Y_PREC 0.001f

// Signal: Z
#define RL_ACCEL_RIDE_HEIGHT_Z_PREC 0.001f

// Signal: Ride Height
#define RL_ACCEL_RIDE_HEIGHT_RIDE_HEIGHT_PREC 0.002f

int pack_rl_accel_ride_height(const msg_rl_accel_ride_height_t* msg, uint8_t* tx_buf);
int unpack_rl_accel_ride_height(const uint8_t* rx_buf, msg_rl_accel_ride_height_t* msg);

// ==========================================================================
// Packet: RL Strain Gauge + Sus Pot. (1285)
// ==========================================================================
// From: CSM
// To:   Pi
#define RL_STRAIN_GAUGE_SUS_POT_ID 1285
#define RL_STRAIN_GAUGE_SUS_POT_DLC 4
#define RL_STRAIN_GAUGE_SUS_POT_FREQ 10
#define RL_STRAIN_GAUGE_SUS_POT_TIMEOUT_MS 20

typedef struct {
    float back_left_strain_gauge_voltage;
    float back_left_suspension_potentiometer;
} msg_rl_strain_gauge_sus_pot_t;

// Signal: Back Left Strain Gauge Voltage
#define RL_STRAIN_GAUGE_SUS_POT_BACK_LEFT_STRAIN_GAUGE_VOLTAGE_PREC 0.0002f

// Signal: Back Left Suspension Potentiometer
#define RL_STRAIN_GAUGE_SUS_POT_BACK_LEFT_SUSPENSION_POTENTIOMETER_PREC 0.001f

int pack_rl_strain_gauge_sus_pot(const msg_rl_strain_gauge_sus_pot_t* msg, uint8_t* tx_buf);
int unpack_rl_strain_gauge_sus_pot(const uint8_t* rx_buf, msg_rl_strain_gauge_sus_pot_t* msg);

// ==========================================================================
// Packet: RR Accel + Ride Height (1286)
// ==========================================================================
// From: CSM
// To:   Pi
#define RR_ACCEL_RIDE_HEIGHT_ID 1286
#define RR_ACCEL_RIDE_HEIGHT_DLC 8
#define RR_ACCEL_RIDE_HEIGHT_FREQ 10
#define RR_ACCEL_RIDE_HEIGHT_TIMEOUT_MS 20

typedef struct {
    float x;
    float y;
    float z;
    float ride_height;
} msg_rr_accel_ride_height_t;

// Signal: X
#define RR_ACCEL_RIDE_HEIGHT_X_PREC 0.001f

// Signal: Y
#define RR_ACCEL_RIDE_HEIGHT_Y_PREC 0.001f

// Signal: Z
#define RR_ACCEL_RIDE_HEIGHT_Z_PREC 0.001f

// Signal: Ride Height
#define RR_ACCEL_RIDE_HEIGHT_RIDE_HEIGHT_PREC 0.002f

int pack_rr_accel_ride_height(const msg_rr_accel_ride_height_t* msg, uint8_t* tx_buf);
int unpack_rr_accel_ride_height(const uint8_t* rx_buf, msg_rr_accel_ride_height_t* msg);

// ==========================================================================
// Packet: RR Strain Gauge + Sus Pot. (1287)
// ==========================================================================
// From: CSM
// To:   Pi
#define RR_STRAIN_GAUGE_SUS_POT_ID 1287
#define RR_STRAIN_GAUGE_SUS_POT_DLC 4
#define RR_STRAIN_GAUGE_SUS_POT_FREQ 10
#define RR_STRAIN_GAUGE_SUS_POT_TIMEOUT_MS 20

typedef struct {
    float back_right_strain_gauge_voltage;
    float back_right_suspension_potentiometer;
} msg_rr_strain_gauge_sus_pot_t;

// Signal: Back Right Strain Gauge Voltage
#define RR_STRAIN_GAUGE_SUS_POT_BACK_RIGHT_STRAIN_GAUGE_VOLTAGE_PREC 0.0002f

// Signal: Back Right Suspension Potentiometer
#define RR_STRAIN_GAUGE_SUS_POT_BACK_RIGHT_SUSPENSION_POTENTIOMETER_PREC 0.001f

int pack_rr_strain_gauge_sus_pot(const msg_rr_strain_gauge_sus_pot_t* msg, uint8_t* tx_buf);
int unpack_rr_strain_gauge_sus_pot(const uint8_t* rx_buf, msg_rr_strain_gauge_sus_pot_t* msg);

// ==========================================================================
// Packet: FL Gyro (1288)
// ==========================================================================
// From: CSM
// To:   Pi
#define FL_GYRO_ID 1288
#define FL_GYRO_DLC 6
#define FL_GYRO_FREQ 10
#define FL_GYRO_TIMEOUT_MS 20

typedef struct {
    float x;
    float y;
    float z;
} msg_fl_gyro_t;

// Signal: X
#define FL_GYRO_X_PREC 0.001f

// Signal: Y
#define FL_GYRO_Y_PREC 0.001f

// Signal: Z
#define FL_GYRO_Z_PREC 0.001f

int pack_fl_gyro(const msg_fl_gyro_t* msg, uint8_t* tx_buf);
int unpack_fl_gyro(const uint8_t* rx_buf, msg_fl_gyro_t* msg);

// ==========================================================================
// Packet: FR Gyro (1289)
// ==========================================================================
// From: CSM
// To:   Pi
#define FR_GYRO_ID 1289
#define FR_GYRO_DLC 6
#define FR_GYRO_FREQ 10
#define FR_GYRO_TIMEOUT_MS 20

typedef struct {
    float x;
    float y;
    float z;
} msg_fr_gyro_t;

// Signal: X
#define FR_GYRO_X_PREC 0.001f

// Signal: Y
#define FR_GYRO_Y_PREC 0.001f

// Signal: Z
#define FR_GYRO_Z_PREC 0.001f

int pack_fr_gyro(const msg_fr_gyro_t* msg, uint8_t* tx_buf);
int unpack_fr_gyro(const uint8_t* rx_buf, msg_fr_gyro_t* msg);

// ==========================================================================
// Packet: RL Gyro (1290)
// ==========================================================================
// From: CSM
// To:   Pi
#define RL_GYRO_ID 1290
#define RL_GYRO_DLC 6
#define RL_GYRO_FREQ 10
#define RL_GYRO_TIMEOUT_MS 20

typedef struct {
    float x;
    float y;
    float z;
} msg_rl_gyro_t;

// Signal: X
#define RL_GYRO_X_PREC 0.001f

// Signal: Y
#define RL_GYRO_Y_PREC 0.001f

// Signal: Z
#define RL_GYRO_Z_PREC 0.001f

int pack_rl_gyro(const msg_rl_gyro_t* msg, uint8_t* tx_buf);
int unpack_rl_gyro(const uint8_t* rx_buf, msg_rl_gyro_t* msg);

// ==========================================================================
// Packet: RR Gyro (1291)
// ==========================================================================
// From: CSM
// To:   Pi
#define RR_GYRO_ID 1291
#define RR_GYRO_DLC 6
#define RR_GYRO_FREQ 10
#define RR_GYRO_TIMEOUT_MS 20

typedef struct {
    float x;
    float y;
    float z;
} msg_rr_gyro_t;

// Signal: X
#define RR_GYRO_X_PREC 0.001f

// Signal: Y
#define RR_GYRO_Y_PREC 0.001f

// Signal: Z
#define RR_GYRO_Z_PREC 0.001f

int pack_rr_gyro(const msg_rr_gyro_t* msg, uint8_t* tx_buf);
int unpack_rr_gyro(const uint8_t* rx_buf, msg_rr_gyro_t* msg);

// ==========================================================================
// Packet: VCU Enter Bootloader (37)
// ==========================================================================
// From: Pi
// To:   VCU
#define VCU_ENTER_BOOTLOADER_ID 37
#define VCU_ENTER_BOOTLOADER_DLC 1
#define VCU_ENTER_BOOTLOADER_FREQ 0
#define VCU_ENTER_BOOTLOADER_TIMEOUT_MS 0

typedef struct {
} msg_vcu_enter_bootloader_t;

int pack_vcu_enter_bootloader(const msg_vcu_enter_bootloader_t* msg, uint8_t* tx_buf);
int unpack_vcu_enter_bootloader(const uint8_t* rx_buf, msg_vcu_enter_bootloader_t* msg);

// ==========================================================================
// Packet: CSM Enter Bootloader (38)
// ==========================================================================
// From: Pi
// To:   CSM
#define CSM_ENTER_BOOTLOADER_ID 38
#define CSM_ENTER_BOOTLOADER_DLC 1
#define CSM_ENTER_BOOTLOADER_FREQ 0
#define CSM_ENTER_BOOTLOADER_TIMEOUT_MS 0

typedef struct {
} msg_csm_enter_bootloader_t;

int pack_csm_enter_bootloader(const msg_csm_enter_bootloader_t* msg, uint8_t* tx_buf);
int unpack_csm_enter_bootloader(const uint8_t* rx_buf, msg_csm_enter_bootloader_t* msg);

// ==========================================================================
// Packet: HVC Enter Bootloader (39)
// ==========================================================================
// From: Pi
// To:   HVC
#define HVC_ENTER_BOOTLOADER_ID 39
#define HVC_ENTER_BOOTLOADER_DLC 1
#define HVC_ENTER_BOOTLOADER_FREQ 0
#define HVC_ENTER_BOOTLOADER_TIMEOUT_MS 0

typedef struct {
} msg_hvc_enter_bootloader_t;

int pack_hvc_enter_bootloader(const msg_hvc_enter_bootloader_t* msg, uint8_t* tx_buf);
int unpack_hvc_enter_bootloader(const uint8_t* rx_buf, msg_hvc_enter_bootloader_t* msg);

// ==========================================================================
// Packet: USM Enter Bootloader (40)
// ==========================================================================
// From: Pi
// To:   USM
#define USM_ENTER_BOOTLOADER_ID 40
#define USM_ENTER_BOOTLOADER_DLC 1
#define USM_ENTER_BOOTLOADER_FREQ 0
#define USM_ENTER_BOOTLOADER_TIMEOUT_MS 0

typedef struct {
} msg_usm_enter_bootloader_t;

int pack_usm_enter_bootloader(const msg_usm_enter_bootloader_t* msg, uint8_t* tx_buf);
int unpack_usm_enter_bootloader(const uint8_t* rx_buf, msg_usm_enter_bootloader_t* msg);

// ==========================================================================
// Packet: HVC Bounds Parameters (48)
// ==========================================================================
// From: Pi
// To:   HVC
#define HVC_BOUNDS_PARAMETERS_ID 48
#define HVC_BOUNDS_PARAMETERS_DLC 0
#define HVC_BOUNDS_PARAMETERS_FREQ 0
#define HVC_BOUNDS_PARAMETERS_TIMEOUT_MS 0

typedef struct {
} msg_hvc_bounds_parameters_t;

int pack_hvc_bounds_parameters(const msg_hvc_bounds_parameters_t* msg, uint8_t* tx_buf);
int unpack_hvc_bounds_parameters(const uint8_t* rx_buf, msg_hvc_bounds_parameters_t* msg);

#endif // CAN_IDS_H
