#include "usm_can.h"

#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "fdcan.h"
#include "longhorn/rtos/can.h"
#include "longhorn/rtos/logger.h"

#include <ota_flash.h>
#include <stm32g4xx_hal_fdcan.h>

/* Generated CAN message definitions */
#include "longhorn/can/can_ids.h"

/* ===============================
   Device ID Mapping
   =============================== */

/* Map BOARD_<location> to DEVICE_ID_USM_<location> */
#if defined(BOARD_FR)
#define THIS_DEVICE_ID DEVICE_ID_USM_FR
#elif defined(BOARD_FL)
#define THIS_DEVICE_ID DEVICE_ID_USM_FL
#elif defined(BOARD_RR)
#define THIS_DEVICE_ID DEVICE_ID_USM_RR
#elif defined(BOARD_RL)
#define THIS_DEVICE_ID DEVICE_ID_USM_RL
#else
#error "USM firmware must be built with one of: BOARD_FR, BOARD_FL, BOARD_RR, BOARD_RL"
#endif

/* ===============================
   CAN Interfaces
   =============================== */

static can_interface_t data_acq_bus;

/* ===============================
   CAN Message Mailboxes
   =============================== */

/* Acceleration Unsprung + Wheel Speed FL (ID 0x402) */
static msg_acceleration_vector_unsprung_wheel_speed_fl_t accel_fl_mailbox = {0};
static can_message_t *accel_fl_handle = NULL;

/* Acceleration Unsprung + Wheel Speed FR (ID 0x403) */
static msg_acceleration_vector_unsprung_wheel_speed_fr_t accel_fr_mailbox = {0};
static can_message_t *accel_fr_handle = NULL;

/* Acceleration Unsprung + Wheel Speed RL (ID 0x404) */
static msg_acceleration_vector_unsprung_wheel_speed_rl_t accel_rl_mailbox = {0};
static can_message_t *accel_rl_handle = NULL;

/* Acceleration Unsprung + Wheel Speed RR (ID 0x405) */
static msg_acceleration_vector_unsprung_wheel_speed_rr_t accel_rr_mailbox = {0};
static can_message_t *accel_rr_handle = NULL;

/* ===============================
   Internal Function Prototypes
   =============================== */

static void usm_can_add_send_handlers(void);

/* ===============================
   CAN Initialization
   =============================== */

void usm_can_init(void) {
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
      .device_id = THIS_DEVICE_ID,
      .write_memory_fn = ota_flash_write_memory,
      .fw_update_begin_fn = ota_flash_begin,
      .abort_update_fn = ota_flash_abort,
  };

  /* USM uses FDCAN2 for data acquisition */
  data_acq_bus.handle = &hfdcan2;
  data_acq_bus.cccr_reg = &hfdcan2.Instance->CCCR;

  can_rtos_init(&cfg);

  /* Register physical interface */
  can_rtos_register_interface(&data_acq_bus);

  /* Register CAN packets before starting interface */
  usm_can_add_send_handlers();

  can_rtos_start_interface(&data_acq_bus);

  /* Start CAN RTOS tasks */
  can_rtos_start_transceiver_task(osPriorityHigh);
  can_rtos_start_receiver_task(osPriorityHigh);

  log_printf(LOG_INFO, "[USM] CAN RTOS initialized (device_id=%d)\n", THIS_DEVICE_ID);
}

/* ===============================
   Send Handler Registration
   =============================== */

static void usm_can_add_send_handlers(void) {

#if defined(BOARD_FL)
  /* Acceleration Unsprung + Wheel Speed FL (ID 0x402) */
  accel_fl_handle = can_get_message_handle(
      &accel_fl_mailbox, ACCELERATION_VECTOR_UNSPRUNG_WHEEL_SPEED_FL_ID,
      ACCELERATION_VECTOR_UNSPRUNG_WHEEL_SPEED_FL_FREQ, ACCELERATION_VECTOR_UNSPRUNG_WHEEL_SPEED_FL_DLC,
      (CAN_pack_message_fn)pack_acceleration_vector_unsprung_wheel_speed_fl);

  can_rtos_register_send_packet(&data_acq_bus, accel_fl_handle);

  log_printf(LOG_INFO,
             "[USM] CAN send handler for acceleration FL registered\n");
#elif defined(BOARD_FR)
  /* Acceleration Unsprung + Wheel Speed FR (ID 0x403) */
  accel_fr_handle = can_get_message_handle(
      &accel_fr_mailbox, ACCELERATION_VECTOR_UNSPRUNG_WHEEL_SPEED_FR_ID,
      ACCELERATION_VECTOR_UNSPRUNG_WHEEL_SPEED_FR_FREQ, ACCELERATION_VECTOR_UNSPRUNG_WHEEL_SPEED_FR_DLC,
      (CAN_pack_message_fn)pack_acceleration_vector_unsprung_wheel_speed_fr);

  can_rtos_register_send_packet(&data_acq_bus, accel_fr_handle);

  log_printf(LOG_INFO,
             "[USM] CAN send handler for acceleration FR registered\n");
#elif defined(BOARD_RL)
  /* Acceleration Unsprung + Wheel Speed RL (ID 0x404) */
  accel_rl_handle = can_get_message_handle(
      &accel_rl_mailbox, ACCELERATION_VECTOR_UNSPRUNG_WHEEL_SPEED_RL_ID,
      ACCELERATION_VECTOR_UNSPRUNG_WHEEL_SPEED_RL_FREQ, ACCELERATION_VECTOR_UNSPRUNG_WHEEL_SPEED_RL_DLC,
      (CAN_pack_message_fn)pack_acceleration_vector_unsprung_wheel_speed_rl);

  can_rtos_register_send_packet(&data_acq_bus, accel_rl_handle);

  log_printf(LOG_INFO,
             "[USM] CAN send handler for acceleration RL registered\n");
#elif defined(BOARD_RR)
  /* Acceleration Unsprung + Wheel Speed RR (ID 0x405) */
  accel_rr_handle = can_get_message_handle(
      &accel_rr_mailbox, ACCELERATION_VECTOR_UNSPRUNG_WHEEL_SPEED_RR_ID,
      ACCELERATION_VECTOR_UNSPRUNG_WHEEL_SPEED_RR_FREQ, ACCELERATION_VECTOR_UNSPRUNG_WHEEL_SPEED_RR_DLC,
      (CAN_pack_message_fn)pack_acceleration_vector_unsprung_wheel_speed_rr);

  can_rtos_register_send_packet(&data_acq_bus, accel_rr_handle);

  log_printf(LOG_INFO,
             "[USM] CAN send handler for acceleration RR registered\n");
#endif
}

/* ===============================
   Packet Update APIs
   =============================== */

/**
 * Update wheel speed for this corner's combined accel + wheel-speed message.
 */
void usm_can_update_wheel_speed(float wheel_speed_rads) {
  taskENTER_CRITICAL();

#if defined(BOARD_FL)
  accel_fl_mailbox.wheel_speed = 67.0f;
#elif defined(BOARD_FR)
  accel_fr_mailbox.wheel_speed = wheel_speed_rads;
#elif defined(BOARD_RL)
  accel_rl_mailbox.wheel_speed = wheel_speed_rads;
#elif defined(BOARD_RR)
  accel_rr_mailbox.wheel_speed = wheel_speed_rads;
#endif

  taskEXIT_CRITICAL();
}

/**
 * Update acceleration for this corner.
 * Each board sends acceleration data for its own corner via a dedicated
 * acceleration message (FL: 1026, FR: 1027, RL: 1028, RR: 1029).
 */
void usm_can_update_accel(float ax, float ay, float az) {
  taskENTER_CRITICAL();

#if defined(BOARD_FL)
  accel_fl_mailbox.x = ax;
  accel_fl_mailbox.y = ay;
  accel_fl_mailbox.z = az;
#elif defined(BOARD_FR)
  accel_fr_mailbox.x = ax;
  accel_fr_mailbox.y = ay;
  accel_fr_mailbox.z = az;
#elif defined(BOARD_RL)
  accel_rl_mailbox.x = ax;
  accel_rl_mailbox.y = ay;
  accel_rl_mailbox.z = az;
#elif defined(BOARD_RR)
  accel_rr_mailbox.x = ax;
  accel_rr_mailbox.y = ay;
  accel_rr_mailbox.z = az;
#endif

  taskEXIT_CRITICAL();
}

void FDCAN2_IT0_IRQHandler(void) {
  HAL_FDCAN_IRQHandler(&hfdcan2);
}

void usm_can_debug(void) {
  log_printf(LOG_INFO, "sent: %lu dropped: %lu err: %d errcode: %d\n",
             data_acq_bus._messages_sent, data_acq_bus.dropped_packets,
             data_acq_bus._error_occurred, data_acq_bus._error_code_send);
}
