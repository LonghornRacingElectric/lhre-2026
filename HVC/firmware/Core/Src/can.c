#include "can.h"

#include <ota_flash.h>
#include <cmsis_os2.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "fdcan.h"
#include "longhorn/can/can_ids.h"
#include "longhorn/rtos/can.h"
#include "longhorn/rtos/logger.h"
#include "task.h"
#include "tim.h"

/* USER CODE BEGIN 0 */

static msg_contactor_status_t contactor_status_tx;

// static msg_vcu_current_sense_t vcu_current_tx;

static msg_indicators_shutdown_status_t indicator_status_tx = {0};

// Cell temperature messages (23 packets total: 22 with 4 temps, 1 with 2 temps)
static msg_cell_temperatures_t cell_temps_tx[23] = {0};

// Charger comms
static msg_charge_command_t   charge_command_tx  = {0};
static msg_charger_status_t   charger_status_rx  = {0};
static can_receive_message_t *charger_status_handle;

can_interface_t critical_can_bus = {
    .handle = &hfdcan1,
};

void hvc_can_init(void) {
  ota_flash_init();

  can_config_t can_config = {
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
      .tick_fn = (Tick_fn)osKernelGetTickCount,
      .add_filter_fn = (CAN_AddFilter_fn)HAL_FDCAN_ConfigFilter,
      .malloc_fn = (Malloc_fn)pvPortMalloc,
      .free_fn = (Free_fn)vPortFree,
      .init_bit = FDCAN_CCCR_INIT,
      .device_id = DEVICE_ID_HVC,
      .write_memory_fn = ota_flash_write_memory,
      .fw_update_begin_fn = ota_flash_begin,
      .abort_update_fn = ota_flash_abort,
  };

  critical_can_bus.cccr_reg = &hfdcan1.Instance->CCCR;

  can_rtos_init(&can_config);
  can_rtos_register_interface(&critical_can_bus);

  // TX: contactor status
  can_message_t *contactor_status_handle = can_get_message_handle(
      &contactor_status_tx, CONTACTOR_STATUS_ID, CONTACTOR_STATUS_FREQ,
      CONTACTOR_STATUS_DLC, (CAN_pack_message_fn)pack_contactor_status);
  can_rtos_register_send_packet(&critical_can_bus, contactor_status_handle);

  // TX: indicators + shutdown status
  can_message_t *indicator_status_handle = can_get_message_handle(
      &indicator_status_tx, INDICATORS_SHUTDOWN_STATUS_ID, 10,
      INDICATORS_SHUTDOWN_STATUS_DLC,
      (CAN_pack_message_fn)pack_indicators_shutdown_status);
  can_rtos_register_send_packet(&critical_can_bus, indicator_status_handle);

  // TX: cell temperatures (23 packets with lower priority via 1000ms frequency)
  for (uint8_t i = 0; i < 23; i++) {
    can_message_t *cell_temp_handle = can_get_message_handle(
        &cell_temps_tx[i], (CELL_TEMPERATURES_ID + i), CELL_TEMPERATURES_FREQ,
        CELL_TEMPERATURES_DLC, (CAN_pack_message_fn)pack_cell_temperatures);
    can_rtos_register_send_packet(&critical_can_bus, cell_temp_handle);
  }

  // TX: charge command (HVC -> Charger)
  can_message_t *charge_command_handle = can_get_message_handle(
      &charge_command_tx, CHARGE_COMMAND_ID, CHARGE_COMMAND_FREQ,
      CHARGE_COMMAND_DLC, (CAN_pack_message_fn)pack_charge_command);
  can_rtos_register_send_packet(&critical_can_bus, charge_command_handle);

  // RX: charger status (Charger -> HVC)
  charger_status_handle = can_get_receive_message_handle(
      &charger_status_rx, CHARGER_STATUS_ID,
      (CAN_unpack_message_fn)unpack_charger_status);
  can_rtos_register_receive_packet(&critical_can_bus, charger_status_handle);

  can_rtos_start_interface(&critical_can_bus);

  // Start tasks LAST
  can_rtos_start_transceiver_task(osPriorityHigh);
  can_rtos_start_receiver_task(osPriorityHigh);

  log_printf(LOG_INFO, "[HVC] CAN RTOS initialized\n");
}

void hvc_set_contactor_status(int state, bool pos, bool neg) {
  taskENTER_CRITICAL();

  contactor_status_tx.hvc_state_machine = (uint8_t)state;
  contactor_status_tx.positive_hv_contactor = pos ? 1 : 0;
  contactor_status_tx.negative_hv_contactor = neg ? 1 : 0;

  taskEXIT_CRITICAL();
}

void hvc_set_indicator_status(bool bms_error, bool imd_error,
                              bool shutdown_leg1, bool shutdown_leg2,
                              bool shutdown_leg3, bool shutdown_leg4) {
  taskENTER_CRITICAL();

  indicator_status_tx.bms_error = bms_error ? 1 : 0;
  indicator_status_tx.imd_error = imd_error ? 1 : 0;
  indicator_status_tx.shutdown_leg_1 = shutdown_leg1 ? 1 : 0;
  indicator_status_tx.shutdown_leg_2 = shutdown_leg2 ? 1 : 0;
  indicator_status_tx.shutdown_leg_3 = shutdown_leg3 ? 1 : 0;
  indicator_status_tx.shutdown_leg_4 = shutdown_leg4 ? 1 : 0;

  taskEXIT_CRITICAL();
}

/**
 * hvc_set_charge_command - Stage the next Charge Command frame for the
 * CAN RTOS to broadcast at CHARGE_COMMAND_FREQ. Called from charger_update().
 */
void hvc_set_charge_command(float target_v, float current_limit_a,
                            uint8_t flags, uint8_t seq)
{
    taskENTER_CRITICAL();
    charge_command_tx.target_voltage = target_v;
    charge_command_tx.current_limit  = current_limit_a;
    charge_command_tx.charge_flags   = flags;
    charge_command_tx.sequence       = seq;
    taskEXIT_CRITICAL();
}

/**
 * hvc_get_charger_status - Atomically copy the most recent Charger Status
 * mailbox into the caller's output params. Polled from charger_update().
 */
void hvc_get_charger_status(float *v_out, float *c_out, uint8_t *flags_out,
                            uint8_t *state_out, uint8_t *plug_out,
                            uint8_t *seq_echo_out) {
    taskENTER_CRITICAL();
    if (v_out)        *v_out        = charger_status_rx.charger_voltage;
    if (c_out)        *c_out        = charger_status_rx.charger_current;
    if (flags_out)    *flags_out    = charger_status_rx.charger_flags;
    if (state_out)    *state_out    = charger_status_rx.charger_state;
    if (plug_out)     *plug_out     = charger_status_rx.plug_status;
    if (seq_echo_out) *seq_echo_out = charger_status_rx.sequence_echo;
    taskEXIT_CRITICAL();
}

/**
 * hvc_set_cell_temperatures - Update cell temperature values for CAN transmission
 * @cell_temps: Array of 90 cell temperature values (float)
 *
 * Distributes 90 temperatures across 23 CAN packets:
 * - Packets 0-21: 4 temperatures each (88 total)
 * - Packet 22: 2 temperatures (last remaining)
 * 
 * The RTOS CAN library handles periodic transmission at CELL_TEMPERATURES_FREQ (1000ms).
 */
void hvc_set_cell_temperatures(float *cell_temps) {
  taskENTER_CRITICAL();

  uint16_t temp_idx = 0;

  // Update 22 packets with 4 temperatures each
  for (uint8_t packet = 0; packet < 22; packet++) {
    cell_temps_tx[packet].temp_i = cell_temps[temp_idx++];
    cell_temps_tx[packet].temp_i_1 = cell_temps[temp_idx++];
    cell_temps_tx[packet].temp_i_2 = cell_temps[temp_idx++];
    cell_temps_tx[packet].temp_i_3 = cell_temps[temp_idx++];
  }

  // Update final packet with 2 remaining temperatures
  cell_temps_tx[22].temp_i = cell_temps[temp_idx++];
  cell_temps_tx[22].temp_i_1 = cell_temps[temp_idx++];
  cell_temps_tx[22].temp_i_2 = 0.0f;  // Unused, set to 0
  cell_temps_tx[22].temp_i_3 = 0.0f;  // Unused, set to 0

  taskEXIT_CRITICAL();
}

/* USER CODE END 0 */