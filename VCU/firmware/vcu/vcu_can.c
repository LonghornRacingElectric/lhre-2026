
#include "vcu_can.h"

#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "fdcan.h"
#include "hvc_states/Core/Inc/hvc_states.h"
#include "longhorn/rtos/can.h"
#include "longhorn/rtos/led.h"
#include "longhorn/rtos/logger.h"
#include <ota_flash.h>
#include <PRNDL.h>
#include <stm32g4xx_hal_fdcan.h>
#include <vcu_inputs.h>

/** ==
 *  CAN Interface and Configuration Setup
 *  ==
 */

can_interface_t critical_bus;
can_interface_t data_acq_bus;

/** ==
 * CAN Packets
 *  ==
 */

/** Receiving */

static msg_contactor_status_t contactor_status_mailbox = {0};
static can_receive_message_t *contactor_status_mailbox_handle = NULL;

static msg_dui_r2d_status_t dui_r2d_status_mailbox = {0};
static can_receive_message_t *dui_r2d_status_mailbox_handle = NULL;

static msg_inverter_status_t inverter_status_mailbox = {0};
static can_receive_message_t *inverter_status_mailbox_handle = NULL;

static msg_inverter_speed_t inverter_speed_mailbox = {0};
static can_receive_message_t *inverter_speed_mailbox_handle = NULL;

static msg_battery_cell_limits_t battery_cell_limits_mailbox = {0};
static can_receive_message_t *battery_cell_limits_mailbox_handle = NULL;

static msg_battery_pack_status_t battery_pack_status_mailbox = {0};
static can_receive_message_t *battery_pack_status_mailbox_handle = NULL;

static msg_inverter_current_t inverter_current_mailbox = {0};
static can_receive_message_t *inverter_current_mailbox_handle = NULL;

static msg_inverter_voltage_t inverter_voltage_mailbox = {0};
static can_receive_message_t *inverter_voltage_mailbox_handle = NULL;

static msg_inverter_faults_t inverter_faults_mailbox = {0};
static can_receive_message_t *inverter_faults_mailbox_handle = NULL;
// #define DUI_R2D_STATUS_TIMEOUT_MS 1000u

// static bool dui_r2d_timed_out_logged = false;

/** Sending */

static msg_inverter_torque_command_t inverter_torque_command_mailbox = {0};
static can_message_t *inverter_torque_command_mailbox_handle = NULL;

static msg_brake_pedal_t brake_pedal_mailbox = {0};
static can_message_t *brake_pedal_mailbox_handle = NULL;

static msg_apps_voltages_t apps_voltages_mailbox = {0};
static can_message_t *apps_voltages_mailbox_handle = NULL;

static msg_accelerator_pedal_t accelerator_pedal_mailbox = {0};
static can_message_t *accelerator_pedal_mailbox_handle = NULL;

static msg_bse_voltages_t bse_voltages_mailbox = {0};
static can_message_t *bse_voltages_mailbox_handle = NULL;

static msg_brakes_t brakes_mailbox = {0};
static can_message_t *brakes_mailbox_handle = NULL;

static msg_steering_column_t steering_column_mailbox = {0};
static can_message_t *steering_column_mailbox_handle = NULL;

static msg_vcu_state_t vcu_state_mailbox = {0};
static can_message_t *vcu_state_mailbox_handle = NULL;

static msg_switch_command_t switch_command_mailbox = {0};
static can_message_t *switch_command_mailbox_handle = NULL;

void vcu_can_add_receive_handlers(void);
void vcu_can_add_send_handlers(void);
void vcu_init_inverter(void);
static float compute_brake_bias_pct(const vcu_outputs_t *out);
static uint8_t pack_apps_faults(const vcu_outputs_t *out);
static uint8_t pack_bse_faults(const vcu_outputs_t *out);

/**
 * @brief Initializes the CAN interface with the RTOS library and registers
 * handlers. Also starts the CAN transceiver and receiver tasks.
 */
void vcu_can_init(void) {
  ota_flash_init();

  can_config_t vcu_can_config = {
      .init_fn = (CAN_Init_fn)HAL_FDCAN_Init,
      .start_fn = (CAN_Start_fn)HAL_FDCAN_Start,
      .noti_fn = (CAN_ActivateNotifications_fn)HAL_FDCAN_ActivateNotification,
      .stop_fn = (CAN_Stop_fn)HAL_FDCAN_Stop,
      .add_to_queue_fn = (CAN_AddToQ_fn)HAL_FDCAN_AddMessageToTxFifoQ,
      .get_tx_fifo_free_level_fn =
          (CAN_GetTxFifoFreeLevel_fn)HAL_FDCAN_GetTxFifoFreeLevel,
      .get_rx_message_fn = (CAN_GetRxMessage_fn)HAL_FDCAN_GetRxMessage,
      .get_rx_fifo_fill_level_fn =
          (CAN_GetRxFifoFillLevel_fn)HAL_FDCAN_GetRxFifoFillLevel,
      .tick_fn = HAL_GetTick,
      .add_filter_fn = (CAN_AddFilter_fn)HAL_FDCAN_ConfigFilter,
      .malloc_fn = pvPortMalloc,
      .free_fn = vPortFree,
      .init_bit = FDCAN_CCCR_INIT,
      .device_id = DEVICE_ID_VCU,
      .write_memory_fn = ota_flash_write_memory,
      .fw_update_begin_fn = ota_flash_begin,
      .abort_update_fn = ota_flash_abort,
  };

  critical_bus.handle = &hfdcan1;
  critical_bus.cccr_reg = &hfdcan1.Instance->CCCR;

  data_acq_bus.handle = &hfdcan2;
  data_acq_bus.cccr_reg = &hfdcan2.Instance->CCCR;

  can_rtos_init(&vcu_can_config);

  // Register physical interfaces (init only, doesn't start the peripheral)
  can_rtos_register_interface(&critical_bus);
  can_rtos_register_interface(&data_acq_bus);

  // Register all send and receive packets BEFORE starting the interfaces.
  // This ensures filters are configured and the TX linked list is complete
  // before the peripheral goes live on the bus.
  vcu_can_add_send_handlers();

  taskENTER_CRITICAL();
  vcu_can_add_receive_handlers();
  taskEXIT_CRITICAL();

  // NOW start the interfaces — peripheral goes live with all filters active
  can_rtos_start_interface(&critical_bus);
  can_rtos_start_interface(&data_acq_bus);

  can_rtos_start_transceiver_task(osPriorityHigh);
  can_rtos_start_receiver_task(osPriorityHigh);

  vcu_init_inverter();

  log_printf(LOG_INFO, "[VCU] CAN RTOS initialized\n");
}

void vcu_can_add_send_handlers(void) {
  inverter_torque_command_mailbox_handle = can_get_message_handle(
      &inverter_torque_command_mailbox, INVERTER_TORQUE_COMMAND_ID,
      INVERTER_TORQUE_COMMAND_FREQ, INVERTER_TORQUE_COMMAND_DLC,
      (CAN_pack_message_fn)pack_inverter_torque_command);
  can_rtos_register_send_packet(&critical_bus,
                                inverter_torque_command_mailbox_handle);
  log_printf(LOG_INFO,
             "[VCU] CAN send handler for inverter torque command registered\n");

  brake_pedal_mailbox_handle = can_get_message_handle(
      &brake_pedal_mailbox, BRAKE_PEDAL_ID, BRAKE_PEDAL_FREQ, BRAKE_PEDAL_DLC,
      (CAN_pack_message_fn)pack_brake_pedal);
  can_rtos_register_send_packet(&critical_bus, brake_pedal_mailbox_handle);
  log_printf(LOG_INFO, "[VCU] CAN send handler for brake pedal registered\n");

  apps_voltages_mailbox_handle =
      can_get_message_handle(&apps_voltages_mailbox, APPS_VOLTAGES_ID,
                             APPS_VOLTAGES_FREQ, APPS_VOLTAGES_DLC,
                             (CAN_pack_message_fn)pack_apps_voltages);
  can_rtos_register_send_packet(&critical_bus, apps_voltages_mailbox_handle);
  log_printf(LOG_INFO, "[VCU] CAN send handler for APPS voltages registered\n");

  accelerator_pedal_mailbox_handle =
      can_get_message_handle(&accelerator_pedal_mailbox, ACCELERATOR_PEDAL_ID,
                             ACCELERATOR_PEDAL_FREQ, ACCELERATOR_PEDAL_DLC,
                             (CAN_pack_message_fn)pack_accelerator_pedal);
  can_rtos_register_send_packet(&critical_bus,
                                accelerator_pedal_mailbox_handle);
  log_printf(LOG_INFO,
             "[VCU] CAN send handler for accelerator pedal registered\n");

  bse_voltages_mailbox_handle =
      can_get_message_handle(&bse_voltages_mailbox, BSE_VOLTAGES_ID,
                             BSE_VOLTAGES_FREQ, BSE_VOLTAGES_DLC,
                             (CAN_pack_message_fn)pack_bse_voltages);
  can_rtos_register_send_packet(&critical_bus, bse_voltages_mailbox_handle);
  log_printf(LOG_INFO, "[VCU] CAN send handler for BSE voltages registered\n");

  brakes_mailbox_handle = can_get_message_handle(
      &brakes_mailbox, BRAKES_ID, BRAKES_FREQ, BRAKES_DLC,
      (CAN_pack_message_fn)pack_brakes);
  can_rtos_register_send_packet(&critical_bus, brakes_mailbox_handle);
  log_printf(LOG_INFO, "[VCU] CAN send handler for brakes registered\n");

  steering_column_mailbox_handle = can_get_message_handle(
      &steering_column_mailbox, STEERING_COLUMN_ID, STEERING_COLUMN_FREQ,
      STEERING_COLUMN_DLC, (CAN_pack_message_fn)pack_steering_column);
  can_rtos_register_send_packet(&critical_bus, steering_column_mailbox_handle);
  log_printf(LOG_INFO,
             "[VCU] CAN send handler for steering column registered\n");

  vcu_state_mailbox_handle =
      can_get_message_handle(&vcu_state_mailbox, VCU_STATE_ID, VCU_STATE_FREQ,
                             VCU_STATE_DLC,
                             (CAN_pack_message_fn)pack_vcu_state);
  can_rtos_register_send_packet(&critical_bus, vcu_state_mailbox_handle);
  log_printf(LOG_INFO, "[VCU] CAN send handler for VCU state registered\n");

  switch_command_mailbox_handle =
      can_get_message_handle(&switch_command_mailbox, SWITCH_COMMAND_ID,
                             SWITCH_COMMAND_FREQ, SWITCH_COMMAND_DLC,
                             (CAN_pack_message_fn)pack_switch_command);
  can_rtos_register_send_packet(&critical_bus, switch_command_mailbox_handle);
  log_printf(LOG_INFO,
             "[VCU] CAN send handler for switch command registered\n");
}

void vcu_can_clear_inverter_faults(void) {
  msg_inverter_parameter_request_t fault_clear = {
      .parameter_address = 20,
      .r_w = 1,
  };
  can_message_t *msg = can_get_message_handle(
      &fault_clear, INVERTER_PARAMETER_REQUEST_ID, 0,
      INVERTER_PARAMETER_REQUEST_DLC,
      (CAN_pack_message_fn)pack_inverter_parameter_request);
  can_rtos_send_immediate(&critical_bus, msg);
}

void vcu_init_inverter() {
  // send can packet with all 0s
  inverter_torque_command_mailbox.torque_request = 0.0f;
  inverter_torque_command_mailbox.enable = 0;
  inverter_torque_command_mailbox.direction = 1;
  inverter_torque_command_mailbox.torque_limit = 0.0f;

  // start actual driving after 100ms
  vTaskDelay(pdMS_TO_TICKS(100));
}

void vcu_can_set_model_inputs(const vcu_inputs_t *in) {
  apps_voltages_mailbox.apps1_voltage = in->apps1_raw;
  apps_voltages_mailbox.apps2_voltage = in->apps2_raw;

  bse_voltages_mailbox.bse_front_voltage = in->bse1_raw;
  bse_voltages_mailbox.bse_rear_voltage = in->bse2_raw;
  bse_voltages_mailbox.bse_line_lock_voltage = 0.0f;
}

void vcu_can_set_model_outputs(const vcu_outputs_t *out) {
  brake_pedal_mailbox.brake_pedal_travel = out->bse_psi_filtered;
  brake_pedal_mailbox.brake_light_percent = out->brake_light_pct;
  brake_pedal_mailbox.bpps_faults = 0;

  inverter_torque_command_mailbox.torque_request = out->torque_cmd;
  inverter_torque_command_mailbox.enable = out->inverter_enable;
  inverter_torque_command_mailbox.torque_limit = 230.0f;
  inverter_torque_command_mailbox.direction = 1;

  apps_voltages_mailbox.apps1_travel = out->apps1_travel;
  apps_voltages_mailbox.apps2_travel = out->apps2_travel;

  accelerator_pedal_mailbox.accelerator_pedal_travel =
      out->accel_pedal_travel;
  accelerator_pedal_mailbox.apps_faults = pack_apps_faults(out);

  brakes_mailbox.brake_pressure_front = out->bse1_psi;
  brakes_mailbox.brake_pressure_rear_pre_lock = out->bse2_psi;
  brakes_mailbox.brake_pressure_rear_post_lock = 0.0f;
  brakes_mailbox.brake_bias = compute_brake_bias_pct(out);
  brakes_mailbox.bse_faults = pack_bse_faults(out);

  vcu_state_mailbox.prndl_state = out->prndl_state;
  vcu_state_mailbox.stomp_fault = out->faults.brake_latched;
  vcu_state_mailbox.ready_to_drive_buzzer = out->buzzer_active;
  vcu_state_mailbox.state_of_charge_estimate = out->soe_pct;
  vcu_state_mailbox.line_lock_enabled = out->linelock_enabled;

  switch_command_mailbox.switch_command = 0u;
  if (out->linelock_enabled) {
    switch_command_mailbox.switch_command |=
        (uint8_t)(1u << SWITCH_COMMAND_SWITCH_COMMAND_TEMP_COMMAND_1_IDX);
  }

  led_set(out->brake_pressed, is_drive_switch_pressed(),
          out->accel_pedal_travel == 0);

  // log_printf(LOG_INFO, "DUI R2D: %d", dui_r2d_status_mailbox.r2d_status);

  // log_printf(LOG_ERROR, "ERROR BITS ON CAN: %d, LEC: %d",
  // hfdcan1.Instance->PSR,
  //            hfdcan1.Instance->PSR & FDCAN_PSR_LEC_Msk);

  // log_printf(LOG_WARNING, "CAN fill level: %d, %d",
  //            HAL_FDCAN_GetRxFifoFillLevel(&hfdcan1, FDCAN_RX_FIFO0),
  //            HAL_FDCAN_GetRxFifoFillLevel(&hfdcan1, FDCAN_RX_FIFO1));

  if (out->prndl_state == PRNDL_DRIVE) {
    led_start_thread();
  } else {
    led_stop_thread();
  }
}

void vcu_can_set_steering_angle_deg(float steering_angle_deg) {
  steering_column_mailbox.steering_column_angle = steering_angle_deg;
}

static float compute_brake_bias_pct(const vcu_outputs_t *out) {
  static float remembered_brake_bias = 0.0f;
  float total = out->bse1_psi + out->bse2_psi;

  if (total <= 1000.0f) {
    return remembered_brake_bias;
  } else {
    remembered_brake_bias = out->bse1_psi / total;
  }
  return remembered_brake_bias;
}

static uint8_t pack_apps_faults(const vcu_outputs_t *out) {
  uint8_t faults = 0;

  if (out->faults.apps1_over_range || out->faults.apps1_under_range) {
    faults |= (uint8_t)(1u << ACCELERATOR_PEDAL_APPS_FAULTS_APPS1_OUT_RANGE_IDX);
  }

  if (out->faults.apps2_over_range || out->faults.apps2_under_range) {
    faults |= (uint8_t)(1u << ACCELERATOR_PEDAL_APPS_FAULTS_APPS2_OUT_RANGE_IDX);
  }

  if (out->faults.apps_implaus) {
    faults |= (uint8_t)(1u << ACCELERATOR_PEDAL_APPS_FAULTS_APPS_MISMATCH_IDX);
    faults |= (uint8_t)(1u << ACCELERATOR_PEDAL_APPS_FAULTS_APPS_IMPLAUSE_IDX);
  }

  return faults;
}

static uint8_t pack_bse_faults(const vcu_outputs_t *out) {
  uint8_t faults = 0;

  if (out->faults.bse1_over_range || out->faults.bse1_under_range) {
    faults |= (uint8_t)(1u << BRAKES_BSE_FAULTS_BSE1_OUT_RANGE_IDX);
  }

  if (out->faults.bse2_over_range || out->faults.bse2_under_range) {
    faults |= (uint8_t)(1u << BRAKES_BSE_FAULTS_BSE2_OUT_RANGE_IDX);
  }

  return faults;
}

bool is_drive_switch_pressed(void) {
  // if (message_timed_out(dui_r2d_status_mailbox_handle,wq
  //   log_printf(LOG_INFO, "[VCU] DUI R2D status restored\n");
  //   dui_r2d_timed_out_logged = false;
  // }

  return dui_r2d_status_mailbox.r2d_status == 1;
}

bool hvc_tractive_ready(void) {
  return contactor_status_mailbox.hvc_state_machine == HVC_STATE_ENERGIZED;
}

float vcu_can_get_motor_speed_rpm(void) {
  if (!message_timed_out(inverter_speed_mailbox_handle,
                         INVERTER_SPEED_TIMEOUT_MS)) {
    return (float)inverter_speed_mailbox.motor_speed;
  }

  return (float)inverter_status_mailbox.motor_speed;
}

float vcu_can_get_delta_resolver_angle_deg(void) {
  return (float)inverter_status_mailbox.delta_resolver_angle;
}

float vcu_can_get_motor_angle_deg(void) {
  return (float)inverter_status_mailbox.motor_angle;
}

float vcu_can_get_min_cell_voltage_v(void) {
  return battery_cell_limits_mailbox.min_cell_voltage;
}

float vcu_can_get_max_cell_voltage_v(void) {
  return battery_cell_limits_mailbox.max_cell_voltage;
}

bool vcu_can_is_motor_speed_valid(void) {
  return !message_timed_out(inverter_speed_mailbox_handle,
                            INVERTER_SPEED_TIMEOUT_MS) ||
         !message_timed_out(inverter_status_mailbox_handle,
                            INVERTER_STATUS_TIMEOUT_MS);
}

bool vcu_can_is_battery_cell_limits_valid(void) {
  return !message_timed_out(battery_cell_limits_mailbox_handle,
                            BATTERY_CELL_LIMITS_TIMEOUT_MS);
}

bool vcu_can_is_inverter_current_valid(void) {
  return !message_timed_out(inverter_current_mailbox_handle,
                            INVERTER_CURRENT_TIMEOUT_MS);
}

bool vcu_can_is_inverter_voltage_valid(void) {
  return !message_timed_out(inverter_voltage_mailbox_handle,
                            INVERTER_VOLTAGE_TIMEOUT_MS);
}

inverter_currents_t vcu_can_get_inverter_currents(void) {
  return (inverter_currents_t){
      .phase_a = inverter_current_mailbox.phase_a_current,
      .phase_b = inverter_current_mailbox.phase_b_current,
      .phase_c = inverter_current_mailbox.phase_c_current,
      .dc_bus  = inverter_current_mailbox.dc_bus_current,
  };
}

inverter_voltages_t vcu_can_get_inverter_voltages(void) {
  return (inverter_voltages_t){
      .dc_bus          = inverter_voltage_mailbox.dc_bus_voltage,
      .neutral_output  = inverter_voltage_mailbox.neutral_output_voltage,
      .vab_vq          = inverter_voltage_mailbox.vab_vq_voltage,
      .vbc_vd          = inverter_voltage_mailbox.vbc_vd_voltage,
  };
}

uint32_t vcu_can_get_inverter_post_faults(void) {
  return inverter_faults_mailbox.post_faults;
}

uint32_t vcu_can_get_inverter_run_faults(void) {
  return inverter_faults_mailbox.run_faults;
}

vcu_battery_pack_status_t vcu_can_get_battery_pack_status(void) {
  const float top_temp_c = (float)battery_pack_status_mailbox.cell_top_temp;
  const float bottom_temp_c = (float)battery_pack_status_mailbox.cell_bottom_temp;
  return (vcu_battery_pack_status_t){
      .pack_voltage_v = battery_pack_status_mailbox.pack_voltage,
      .state_of_charge_pct = battery_pack_status_mailbox.state_of_charge,
      .min_cell_temp_c = top_temp_c < bottom_temp_c ? top_temp_c : bottom_temp_c,
      .max_cell_temp_c = top_temp_c > bottom_temp_c ? top_temp_c : bottom_temp_c,
      .valid = !message_timed_out(battery_pack_status_mailbox_handle,
                                  BATTERY_PACK_STATUS_TIMEOUT_MS),
  };
}

/**
 * @brief Creates the CAN receive handlers and registers them with the CAN lib
 *
 * Forward declaration is above, but this API is NOT public, so don't call in
 * other files
 *
 */
void vcu_can_add_receive_handlers(void) {
  contactor_status_mailbox_handle = can_get_receive_message_handle(
      &contactor_status_mailbox, CONTACTOR_STATUS_ID,
      (CAN_unpack_message_fn)unpack_contactor_status);
  can_rtos_register_receive_packet(&critical_bus,
                                   contactor_status_mailbox_handle);
  log_printf(LOG_INFO,
             "[VCU] CAN receive handler for contactor status registered\n");

  dui_r2d_status_mailbox_handle = can_get_receive_message_handle(
      &dui_r2d_status_mailbox, DUI_R2D_STATUS_ID,
      (CAN_unpack_message_fn)unpack_dui_r2d_status);
  can_rtos_register_receive_packet(&critical_bus,
                                   dui_r2d_status_mailbox_handle);
  log_printf(LOG_INFO,
             "[VCU] CAN receive handler for DUI R2D status registered\n");

  inverter_status_mailbox_handle = can_get_receive_message_handle(
      &inverter_status_mailbox, INVERTER_STATUS_ID,
      (CAN_unpack_message_fn)unpack_inverter_status);
  can_rtos_register_receive_packet(&critical_bus,
                                   inverter_status_mailbox_handle);
  log_printf(LOG_INFO,
             "[VCU] CAN receive handler for inverter status registered\n");

  inverter_speed_mailbox_handle = can_get_receive_message_handle(
      &inverter_speed_mailbox, INVERTER_SPEED_ID,
      (CAN_unpack_message_fn)unpack_inverter_speed);
  can_rtos_register_receive_packet(&critical_bus,
                                   inverter_speed_mailbox_handle);

  log_printf(LOG_INFO,
             "[VCU] CAN receive handler for inverter speed registered\n");

  battery_cell_limits_mailbox_handle = can_get_receive_message_handle(
      &battery_cell_limits_mailbox, BATTERY_CELL_LIMITS_ID,
      (CAN_unpack_message_fn)unpack_battery_cell_limits);
  can_rtos_register_receive_packet(&critical_bus,
                                   battery_cell_limits_mailbox_handle);
  log_printf(LOG_INFO,
             "[VCU] CAN receive handler for battery cell limits registered\n");

  battery_pack_status_mailbox_handle = can_get_receive_message_handle(
      &battery_pack_status_mailbox, BATTERY_PACK_STATUS_ID,
      (CAN_unpack_message_fn)unpack_battery_pack_status);
  can_rtos_register_receive_packet(&critical_bus,
                                   battery_pack_status_mailbox_handle);
  log_printf(LOG_INFO,
             "[VCU] CAN receive handler for battery pack status registered\n");

  inverter_current_mailbox_handle = can_get_receive_message_handle(
      &inverter_current_mailbox, INVERTER_CURRENT_ID,
      (CAN_unpack_message_fn)unpack_inverter_current);
  can_rtos_register_receive_packet(&critical_bus,
                                   inverter_current_mailbox_handle);
  log_printf(LOG_INFO,
             "[VCU] CAN receive handler for inverter current registered\n");

  inverter_voltage_mailbox_handle = can_get_receive_message_handle(
      &inverter_voltage_mailbox, INVERTER_VOLTAGE_ID,
      (CAN_unpack_message_fn)unpack_inverter_voltage);
  can_rtos_register_receive_packet(&critical_bus,
                                   inverter_voltage_mailbox_handle);
  log_printf(LOG_INFO,
             "[VCU] CAN receive handler for inverter voltage registered\n");

  inverter_faults_mailbox_handle = can_get_receive_message_handle(
      &inverter_faults_mailbox, INVERTER_FAULTS_ID,
      (CAN_unpack_message_fn)unpack_inverter_faults);
  can_rtos_register_receive_packet(&critical_bus,
                                   inverter_faults_mailbox_handle);
  log_printf(LOG_INFO,
             "[VCU] CAN receive handler for inverter faults registered\n");
}
