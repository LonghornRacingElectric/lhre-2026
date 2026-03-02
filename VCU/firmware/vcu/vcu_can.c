
#include "vcu_can.h"

#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "fdcan.h"
#include "hvc_states/Core/Inc/hvc_states.h"
#include "longhorn/rtos/can.h"
#include "longhorn/rtos/logger.h"
#include <stm32g4xx_hal_fdcan.h>

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

/** Sending */

static msg_inverter_torque_command_t inverter_torque_command_mailbox = {0};
static can_message_t *inverter_torque_command_mailbox_handle = NULL;

static msg_brake_pedal_t brake_pedal_mailbox = {0};
static can_message_t *brake_pedal_mailbox_handle = NULL;

void vcu_can_add_receive_handlers(void);
void vcu_can_add_send_handlers(void);
void vcu_init_inverter(void);

/**
 * @brief Initializes the CAN interface with the RTOS library and registers
 * handlers. Also starts the CAN transceiver and receiver tasks.
 */
void vcu_can_init(void) {
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

  can_rtos_start_transceiver_task(osPriorityNormal);
  can_rtos_start_receiver_task(osPriorityAboveNormal);

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

void vcu_can_set_model_outputs(vcu_outputs_t *out) {
  // TODO: use BPPS instead of BSE for this.
  brake_pedal_mailbox.brake_pedal_travel = out->bse_psi_filtered;
  brake_pedal_mailbox.brake_light_percent = out->brake_light_pct;

  inverter_torque_command_mailbox.torque_request = out->torque_cmd;
  inverter_torque_command_mailbox.enable = out->inverter_enable;
  inverter_torque_command_mailbox.torque_limit = 200.0f;
  inverter_torque_command_mailbox.direction = 1;
}

bool is_drive_switch_pressed(void) {
  return dui_r2d_status_mailbox.r2d_status == 1;
}

bool hvc_tractive_ready(void) {
  return contactor_status_mailbox.hvc_state_machine == HVC_STATE_ENERGIZED;
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
}

// #include "vcu_can.h"
// #include "FreeRTOS.h"
// #include "cmsis_os.h"
// #include "fdcan.h"
// #include "longhorn/can/can_ids.h"
// #include "longhorn/rtos/can.h"
// #include "longhorn/rtos/logger.h"
// #include "task.h"

// #include <string.h>

// // 0x0C0  (VCU → Inverter)  TORQUE COMMAND
// // Byte 0–1 : torque_request   (int16, 0.1 Nm units)
// // Byte 2–3 : rpm_request      (int16, rpm)
// // Byte 4   : direction        (uint8, 1=fwd, 0=rev)
// // Byte 5   : enable           (uint8, 0=disable, 1=enable)
// // Byte 6–7 : torque_limit     (int16, 0.1 Nm)

// // 0x0B0  (Inverter → VCU)  SPEED / TORQUE FEEDBACK
// // Byte 0–1 : torque_command   (int16, 0.1 Nm)
// // Byte 2–3 : torque_feedback  (int16, 0.1 Nm)
// // Byte 4–5 : motor_speed      (int16, rpm)
// // Byte 6–7 : bus_voltage      (uint16, 0.1 V)

// static can_interface_t can1 = {
//     .handle = &hfdcan1,
// };

// static can_interface_t can2 = {
//     .handle = &hfdcan2,
// };

// // TX: inverter torque command
// static msg_inverter_torque_command_t inv_tx;
// static can_message_t *inv_tx_handle;

// // RX: inverter speed feedback
// static msg_inverter_speed_t inv_speed;
// static can_receive_message_t *inv_speed_rx;

// // RX: contactor status (0x203)
// static msg_contactor_status_t contactor_status;
// static can_receive_message_t *contactor_status_rx;

// // Public feedback values
// float inverter_torque_fb = 0.0f;
// int16_t inverter_rpm = 0;
// float inverter_bus_voltage = 0.0f;

// // Derived HV state
// bool hv_contactors_closed = false;
// int hvc_state = 0;

// // RTOS CAN configuration
// static can_config_t cfg = {
//     .init_fn = (CAN_Init_fn)HAL_FDCAN_Init,
//     .start_fn = (CAN_Start_fn)HAL_FDCAN_Start,
//     .noti_fn = (CAN_ActivateNotifications_fn)HAL_FDCAN_ActivateNotification,
//     .stop_fn = (CAN_Stop_fn)HAL_FDCAN_Stop,
//     .add_to_queue_fn = (CAN_AddToQ_fn)HAL_FDCAN_AddMessageToTxFifoQ,
//     .get_rx_message_fn = (CAN_GetRxMessage_fn)HAL_FDCAN_GetRxMessage,
//     .tick_fn = HAL_GetTick,
//     .add_filter_fn = (CAN_AddFilter_fn)HAL_FDCAN_ConfigFilter,
//     .malloc_fn = pvPortMalloc,
//     .free_fn = vPortFree,
// };

// void vcu_can_init(void) {
//   can_rtos_init(&cfg);

//   // Register physical interface FIRST
//   can_rtos_register_interface(&can2);

//   // TX: inverter torque command
//   memset(&inv_tx, 0, sizeof(inv_tx));
//   inv_tx.direction = 1;
//   inv_tx.enable = 0; // gated by contactors
//   inv_tx.rpm_request = 0;
//   inv_tx.torque_limit = TABLE_SPIN_TORQUE_LIMIT_NM;

//   inv_tx_handle = can_get_message_handle(
//       &inv_tx, INVERTER_TORQUE_COMMAND_ID, INVERTER_TORQUE_COMMAND_FREQ,
//       INVERTER_TORQUE_COMMAND_DLC,
//       (CAN_pack_message_fn)pack_inverter_torque_command);
//   can_rtos_register_send_packet(&can2, inv_tx_handle);

//   // RX: inverter speed
//   inv_speed_rx = can_get_receive_message_handle(
//       &inv_speed, INVERTER_SPEED_ID,
//       (CAN_unpack_message_fn)unpack_inverter_speed);
//   can_rtos_register_receive_packet(&can2, inv_speed_rx);

//   // RX: contactor status
//   contactor_status_rx = can_get_receive_message_handle(
//       &contactor_status, CONTACTOR_STATUS_ID,
//       (CAN_unpack_message_fn)unpack_contactor_status);
//   can_rtos_register_receive_packet(&can2, contactor_status_rx);

//   // Start tasks LAST
//   can_rtos_start_transceiver_task(osPriorityNormal);
//   can_rtos_start_receiver_task(osPriorityAboveNormal);

//   log_printf(LOG_INFO, "[VCU] CAN RTOS initialized\n");
// }

// void vcu_can_set_torque(float torque_nm) {
//   taskENTER_CRITICAL();

//   if (torque_nm < 0.0f) {
//     torque_nm = 0.0f;
//   }

//   if (torque_nm > TABLE_SPIN_TORQUE_LIMIT_NM) {
//     torque_nm = TABLE_SPIN_TORQUE_LIMIT_NM;
//   }

//   inv_tx.torque_request = torque_nm;
//   inv_tx.torque_limit = TABLE_SPIN_TORQUE_LIMIT_NM;

//   // Enable only if contactors are closed
//   // inv_tx.enable = hv_contactors_closed && (inv_tx.torque_request > 0.0f);
//   inv_tx.enable = (inv_tx.torque_request > 0.0f);

//   taskEXIT_CRITICAL();
// }

// void vcu_can_read_feedback(void) {
//   msg_inverter_speed_t local;

//   taskENTER_CRITICAL();
//   local = inv_speed;
//   taskEXIT_CRITICAL();

//   inverter_torque_fb = local.torque_feedback; // Nm (already scaled)
//   inverter_bus_voltage = local.bus_voltage;   // V
//   inverter_rpm = local.motor_speed;           // rpm
// }

// void vcu_can_read_contactor_status(void) {
//   msg_contactor_status_t local;

//   taskENTER_CRITICAL();
//   local = contactor_status;
//   taskEXIT_CRITICAL();

//   hvc_state = (int)local.hvc_state_machine;

//   hv_contactors_closed =
//       local.positive_hv_contactor && local.negative_hv_contactor;
// }