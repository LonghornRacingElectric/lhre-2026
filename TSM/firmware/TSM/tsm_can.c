#include "tsm_can.h"

#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "fdcan.h"

#include "longhorn/rtos/can.h"
#include "longhorn/rtos/logger.h"

#include <ota_flash.h>
#include <stm32g4xx_hal_fdcan.h>

/* ===============================
   CAN Interface
   =============================== */

static can_interface_t data_acq_bus;

/* ===============================
   CAN Sensor Packet
   =============================== */

static msg_tsm_sensors_t tsm_sensors_mailbox = {0};
static can_message_t *tsm_sensors_mailbox_handle = NULL;

/* ===============================
   CAN Init
   =============================== */

static void tsm_can_add_send_handlers(void);

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

  data_acq_bus.handle = &hfdcan2;
  data_acq_bus.cccr_reg = &hfdcan2.Instance->CCCR;

  can_rtos_init(&cfg);

  /* Register physical interface */
  can_rtos_register_interface(&data_acq_bus);

  /* Register packets BEFORE starting interface */
  tsm_can_add_send_handlers();

  /* Start CAN */
  can_rtos_start_interface(&data_acq_bus);

  /* Start RTOS CAN tasks */
  can_rtos_start_transceiver_task(osPriorityHigh);
  can_rtos_start_receiver_task(osPriorityHigh);

  log_printf(LOG_INFO, "[TSM] CAN initialized\n");
}

// Send handlers

static void tsm_can_add_send_handlers(void) {

  tsm_sensors_mailbox_handle = can_get_message_handle(
      &tsm_sensors_mailbox, TSM_SENSORS_ID, TSM_SENSORS_FREQ, TSM_SENSORS_DLC,
      (CAN_pack_message_fn)pack_tsm_sensors);

  can_rtos_register_send_packet(&data_acq_bus, tsm_sensors_mailbox_handle);

  log_printf(LOG_INFO, "[TSM] Sensor CAN packet registered\n");
}

//  Sensor Update

void tsm_can_update_sensors(float therm1, float therm2, float therm3,
                            float therm4, float coolant_flow_lpm,
                            float fan_rpm) {

  taskENTER_CRITICAL();

  tsm_sensors_mailbox.thermistor1 = therm1;
  tsm_sensors_mailbox.thermistor2 = therm2;
  tsm_sensors_mailbox.thermistor3 = therm3;
  tsm_sensors_mailbox.thermistor4 = therm4;

  tsm_sensors_mailbox.coolant_flow_lpm = coolant_flow_lpm;
  tsm_sensors_mailbox.fan_rpm = fan_rpm;

  taskEXIT_CRITICAL();
}