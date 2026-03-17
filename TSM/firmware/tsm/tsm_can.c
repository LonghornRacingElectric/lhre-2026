#include "tsm_can.h"

#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "fdcan.h"
#include "longhorn/rtos/can.h"
#include "longhorn/rtos/logger.h"

#include <ota_flash.h>
#include <stm32g4xx_hal_fdcan.h>

/* ===============================
   CAN Interfaces
   =============================== */

static can_interface_t data_acq_bus;

/* ===============================
   CAN Mailboxes
   =============================== */

/* Coolant loop temps (ID 416) */
static msg_coolant_loop_temps_t coolant_loop_mailbox = {0};
static can_message_t *coolant_loop_handle = NULL;

/* Cooling system sensors (ID 418) */
static msg_tsm_cooling_system_t cooling_sys_mailbox = {0};
static can_message_t *cooling_sys_handle = NULL;

/* ===============================
   Internal Function Prototypes
   =============================== */

static void tsm_can_add_send_handlers(void);

/* ===============================
   CAN Initialization
   =============================== */

void tsm_can_init(void) {
  ota_flash_init();

  can_config_t cfg = {
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
      .device_id = DEVICE_ID_TSM,
      .write_memory_fn = ota_flash_write_memory,
      .fw_update_begin_fn = ota_flash_begin,
      .abort_update_fn = ota_flash_abort,
  };

  /* TSM uses CAN2 for data acquisition */
  data_acq_bus.handle = &hfdcan2;
  data_acq_bus.cccr_reg = &hfdcan2.Instance->CCCR;

  can_rtos_init(&cfg);

  /* Register physical interface */
  can_rtos_register_interface(&data_acq_bus);

  /* Register CAN packets before starting interface */
  tsm_can_add_send_handlers();

  /* Start CAN interface */

  HAL_StatusTypeDef status = HAL_FDCAN_Start(&hfdcan2);
  //   for (int i = 0; i < 10; i++) {
  //     log_printf(LOG_INFO, "FDCAN start status: %d\n", status);
  //     osDelay(500);
  //   }

  can_rtos_start_interface(&data_acq_bus);
  //   log_printf(LOG_INFO, "FDCAN start status: %d\n", status);
  //   osDelay(2000);
  //   log_printf(LOG_INFO, "data_acq_bus head: %p\n", data_acq_bus._head);
  //   log_printf(LOG_INFO, "coolant handle: %p scheduled: %d\n",
  //              coolant_loop_handle,
  //              coolant_loop_handle ? coolant_loop_handle->_is_scheduled :
  //              -1);

  //   osDelay(1000);
  //   can_message_t *test = coolant_loop_handle;
  //   cHAL_StatusTypeDef result = can_rtos_send_immediate(&data_acq_bus, test);
  //   log_printf(LOG_INFO, "force send result: %d\n", result);
  //   log_printf(LOG_INFO, "FDCAN PSR: 0x%08lX\n", hfdcan2.Instance->PSR);

  /* Start CAN RTOS tasks */
  can_rtos_start_transceiver_task(osPriorityHigh);
  can_rtos_start_receiver_task(osPriorityHigh);

  log_printf(LOG_INFO, "[TSM] CAN RTOS initialized\n");
}

/* ===============================
   Send Handler Registration
   =============================== */

static void tsm_can_add_send_handlers(void) {
  /* Coolant Loop Temps (ID 416) */
  coolant_loop_handle = can_get_message_handle(
      &coolant_loop_mailbox, COOLANT_LOOP_TEMPS_ID, COOLANT_LOOP_TEMPS_FREQ,
      COOLANT_LOOP_TEMPS_DLC, (CAN_pack_message_fn)pack_coolant_loop_temps);

  can_rtos_register_send_packet(&data_acq_bus, coolant_loop_handle);

  log_printf(LOG_INFO,
             "[TSM] CAN send handler for coolant loop temps registered\n");

  /* Cooling System (ID 418) */
  cooling_sys_handle = can_get_message_handle(
      &cooling_sys_mailbox, TSM_COOLING_SYSTEM_ID, TSM_COOLING_SYSTEM_FREQ,
      TSM_COOLING_SYSTEM_DLC, (CAN_pack_message_fn)pack_tsm_cooling_system);

  can_rtos_register_send_packet(&data_acq_bus, cooling_sys_handle);

  log_printf(LOG_INFO,
             "[TSM] CAN send handler for cooling system registered\n");
}

/* ===============================
   Packet Update APIs
   =============================== */

void tsm_can_update_coolant_loop(float loop_motor_temp,
                                 float loop_inverter_temp,
                                 float radiator_outlet_temp) {
  taskENTER_CRITICAL();

  coolant_loop_mailbox.loop_temp_after_motor = loop_motor_temp;
  coolant_loop_mailbox.loop_temp_after_inverter = loop_inverter_temp;
  coolant_loop_mailbox.temp_after_radiator = radiator_outlet_temp;

  taskEXIT_CRITICAL();
}

void tsm_can_update_cooling_system(float fan_rpm, float coolant_flow_lpm,
                                   float ambient_temp) {
  taskENTER_CRITICAL();

  cooling_sys_mailbox.radiator_fan_speed = (uint16_t)fan_rpm;
  cooling_sys_mailbox.coolant_flow = coolant_flow_lpm;
  cooling_sys_mailbox.ambient_temp_ds18b20 = ambient_temp;

  taskEXIT_CRITICAL();
}

void tsm_can_debug(void) {
  log_printf(LOG_INFO, "sent: %lu dropped: %lu err: %d errcode: %d\n",
             data_acq_bus._messages_sent, data_acq_bus.dropped_packets,
             data_acq_bus._error_occurred, data_acq_bus._error_code_send);
}