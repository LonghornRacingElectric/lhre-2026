#include "dui_can.h"

#include "cmsis_os.h"
#include "fdcan.h"
#include "longhorn/fw_update.h"
#include "longhorn/rtos/can.h"
#include "longhorn/rtos/logger.h"
#include "ota/ota_flash.h"

/** ==
 *  CAN Interface and Configuration Setup
 *  ==
 */

static can_interface_t critical_bus = {
    .handle = &hfdcan1,
};

/** ==
 * Outgoing CAN Packets
 *  ==
 */

static msg_dui_r2d_status_t r2d_status_mailbox = {0};
static can_message_t *r2d_status_mailbox_handle = NULL;

/** ==
 * Incoming CAN Packets
 *  ==
 */

static msg_indicators_shutdown_status_t indicator_status_mailbox = {0};
static can_receive_message_t *indicator_status_mailbox_handle = NULL;

static msg_vcu_state_t vcu_state_mailbox = {0};
static can_receive_message_t *vcu_state_mailbox_handle = NULL;

void dui_can_add_receive_handlers(void);
void dui_can_add_send_handlers(void);

/**
 * @brief Initializes the CAN interface with the RTOS library and registers
 * handlers. Also starts the CAN transceiver and receiver tasks.
 */
void dui_can_init(void) {
  ota_flash_init();

  can_config_t dui_can_config = {
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
      .device_id = DEVICE_ID_DUI,
      .write_memory_fn = ota_flash_write_memory,
      .fw_update_begin_fn = ota_flash_begin,
      .abort_update_fn = ota_flash_abort,
  };

  can_rtos_init(&dui_can_config);

  critical_bus.cccr_reg = &hfdcan1.Instance->CCCR;

  // Register physical interfaces FIRST
  can_rtos_register_interface(&critical_bus);

  dui_can_add_send_handlers();
  taskENTER_CRITICAL();
  dui_can_add_receive_handlers();
  taskEXIT_CRITICAL();

  can_rtos_start_interface(&critical_bus);

  can_rtos_start_transceiver_task(osPriorityNormal);
  can_rtos_start_receiver_task(osPriorityAboveNormal);

  log_printf(LOG_INFO, "[DUI] CAN RTOS initialized\n");
}

/**
 * @brief Creates the CAN receive handlers and registers them with the CAN lib
 *
 * Forward declaration is above, but this API is NOT public, so don't call in
 * other files
 *
 */
void dui_can_add_receive_handlers(void) {
  // Indicators + Shutdown Status
  indicator_status_mailbox_handle = can_get_receive_message_handle(
      &indicator_status_mailbox, INDICATORS_SHUTDOWN_STATUS_ID,
      (CAN_unpack_message_fn)unpack_indicators_shutdown_status);

  can_rtos_register_receive_packet(&critical_bus,
                                   indicator_status_mailbox_handle);

  log_printf(LOG_INFO, "[DUI] CAN IMD + BMS handlers registered\n");

  // VCU State
  vcu_state_mailbox_handle = can_get_receive_message_handle(
      &vcu_state_mailbox, VCU_STATE_ID, (CAN_unpack_message_fn)unpack_vcu_state);

  can_rtos_register_receive_packet(&critical_bus, vcu_state_mailbox_handle);

  log_printf(LOG_INFO, "[DUI] CAN VCU State handler registered\n");
}

/**
 * @brief Creates the CAN send handlers and registers them with the CAN lib
 *
 * Forward declaration is above, but this API is NOT public, so don't call in
 * other files
 *
 */
void dui_can_add_send_handlers(void) {
  r2d_status_mailbox_handle = can_get_message_handle(
      &r2d_status_mailbox, DUI_R2D_STATUS_ID, DUI_R2D_STATUS_FREQ,
      DUI_R2D_STATUS_DLC, (CAN_pack_message_fn)pack_dui_r2d_status);
  can_rtos_register_send_packet(&critical_bus, r2d_status_mailbox_handle);
}

/**
 * @brief Check if there are faults. Checks the mailbox that is updated by the
 * CAN RTOS tasks. Then, returns true if there is a fault present OR if there
 * was a timeout.
 *
 * @return true
 * @return false
 */
bool hvc_imd_fault(void) {
  return indicator_status_mailbox.imd_error ||
         message_timed_out(indicator_status_mailbox_handle,
                           INDICATORS_SHUTDOWN_STATUS_TIMEOUT_MS * 4);
}

bool hvc_bms_fault(void) {
  return indicator_status_mailbox.bms_error ||
         message_timed_out(indicator_status_mailbox_handle,
                           INDICATORS_SHUTDOWN_STATUS_TIMEOUT_MS * 4);
}

void dui_set_r2d(bool enabled) { r2d_status_mailbox.r2d_status = enabled; }

bool dui_r2d_buzzer_active(void) {
  return vcu_state_mailbox.ready_to_drive_buzzer;
}

dui_can_debug_state_t dui_can_get_debug_state(void) {
  dui_can_debug_state_t state = {
      .r2d_switch = r2d_status_mailbox.r2d_status,
      .prndl_state = vcu_state_mailbox.prndl_state,
      .ready_to_drive_buzzer = vcu_state_mailbox.ready_to_drive_buzzer,
      .vcu_state_timed_out =
          message_timed_out(vcu_state_mailbox_handle, VCU_STATE_TIMEOUT_MS * 4),
  };

  state.buzzer_active = state.ready_to_drive_buzzer;

  return state;
}
