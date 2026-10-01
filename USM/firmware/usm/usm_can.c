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
#include "longhorn/traction_status.h"
#include "orion_time_us.h"
#include "wheel_calibration.h"
#include <math.h>

#if defined(BOARD_FL)
#define PHASE_TYPE msg_traction_wheel_fl_t
#define PHASE_ID TRACTION_WHEEL_FL_ID
#define PHASE_PACK pack_traction_wheel_fl
#elif defined(BOARD_FR)
#define PHASE_TYPE msg_traction_wheel_fr_t
#define PHASE_ID TRACTION_WHEEL_FR_ID
#define PHASE_PACK pack_traction_wheel_fr
#elif defined(BOARD_RL)
#define PHASE_TYPE msg_traction_wheel_rl_t
#define PHASE_ID TRACTION_WHEEL_RL_ID
#define PHASE_PACK pack_traction_wheel_rl
#else
#define PHASE_TYPE msg_traction_wheel_rr_t
#define PHASE_ID TRACTION_WHEEL_RR_ID
#define PHASE_PACK pack_traction_wheel_rr
#endif
static PHASE_TYPE phase_mailbox;
static msg_traction_clock_sync_t sync_mailbox;
static can_receive_message_t *sync_handle;
static uint32_t sync_offset, sync_received_us;
static uint16_t sync_sequence;
static bool have_sync, sync_qualified;

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

/* Acceleration Unsprung FL (ID 1026) */
static msg_acceleration_vector_unsprung_wheel_speed_fl_t accel_fl_mailbox = {0};
static can_message_t *accel_fl_handle = NULL;

/* Acceleration Unsprung FR (ID 1027) */
static msg_acceleration_vector_unsprung_wheel_speed_fr_t accel_fr_mailbox = {0};
static can_message_t *accel_fr_handle = NULL;

/* Acceleration Unsprung RL (ID 1028) */
static msg_acceleration_vector_unsprung_wheel_speed_rl_t accel_rl_mailbox = {0};
static can_message_t *accel_rl_handle = NULL;

/* Acceleration Unsprung RR (ID 1029) */
static msg_acceleration_vector_unsprung_wheel_speed_rr_t accel_rr_mailbox = {0};
static can_message_t *accel_rr_handle = NULL;

/* ===============================
   Internal Function Prototypes
   =============================== */

static int pack_phase_snapshot(const void *message, uint8_t *data) {
  (void)message;
  taskENTER_CRITICAL();
  PHASE_TYPE snapshot = phase_mailbox;
  taskEXIT_CRITICAL();
  return PHASE_PACK(&snapshot, data);
}

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
      .tick_us_fn = orion_time_us,
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
  can_message_t *phase_handle = can_get_message_handle(
      &phase_mailbox, PHASE_ID, 3, 8, (CAN_pack_message_fn)pack_phase_snapshot);
  can_rtos_register_send_packet(&data_acq_bus, phase_handle);
  sync_handle = can_get_receive_message_handle(
      &sync_mailbox, TRACTION_CLOCK_SYNC_ID,
      (CAN_unpack_message_fn)unpack_traction_clock_sync);
  sync_handle->expected_dlc = 8;
  can_rtos_register_receive_packet(&data_acq_bus, sync_handle);

  /* Start CAN interface */
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
  /* Acceleration Unsprung FL (ID 1026) */
  accel_fl_handle = can_get_message_handle(
      &accel_fl_mailbox, ACCELERATION_VECTOR_UNSPRUNG_WHEEL_SPEED_FL_ID,
      ACCELERATION_VECTOR_UNSPRUNG_WHEEL_SPEED_FL_FREQ, ACCELERATION_VECTOR_UNSPRUNG_WHEEL_SPEED_FL_DLC,
      (CAN_pack_message_fn)pack_acceleration_vector_unsprung_wheel_speed_fl);

  can_rtos_register_send_packet(&data_acq_bus, accel_fl_handle);

  log_printf(LOG_INFO,
             "[USM] CAN send handler for acceleration FL registered\n");
#elif defined(BOARD_FR)
  /* Acceleration Unsprung FR (ID 1027) */
  accel_fr_handle = can_get_message_handle(
      &accel_fr_mailbox, ACCELERATION_VECTOR_UNSPRUNG_WHEEL_SPEED_FR_ID,
      ACCELERATION_VECTOR_UNSPRUNG_WHEEL_SPEED_FR_FREQ, ACCELERATION_VECTOR_UNSPRUNG_WHEEL_SPEED_FR_DLC,
      (CAN_pack_message_fn)pack_acceleration_vector_unsprung_wheel_speed_fr);

  can_rtos_register_send_packet(&data_acq_bus, accel_fr_handle);

  log_printf(LOG_INFO,
             "[USM] CAN send handler for acceleration FR registered\n");
#elif defined(BOARD_RL)
  /* Acceleration Unsprung RL (ID 1028) */
  accel_rl_handle = can_get_message_handle(
      &accel_rl_mailbox, ACCELERATION_VECTOR_UNSPRUNG_WHEEL_SPEED_RL_ID,
      ACCELERATION_VECTOR_UNSPRUNG_WHEEL_SPEED_RL_FREQ, ACCELERATION_VECTOR_UNSPRUNG_WHEEL_SPEED_RL_DLC,
      (CAN_pack_message_fn)pack_acceleration_vector_unsprung_wheel_speed_rl);

  can_rtos_register_send_packet(&data_acq_bus, accel_rl_handle);

  log_printf(LOG_INFO,
             "[USM] CAN send handler for acceleration RL registered\n");
#elif defined(BOARD_RR)
  /* Acceleration Unsprung RR (ID 1029) */
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
 * Update wheel speed for this corner and send all 4 zeros for other corners.
 * The CAN message (ID 1024) includes speeds for all 4 corners, so we set
 * the appropriate corner based on the board location and zero the others.
 */
void usm_can_update_wheel_speed(float wheel_speed_rads) {
  taskENTER_CRITICAL();

#if defined(BOARD_FL)
  accel_fl_mailbox.wheel_speed = wheel_speed_rads;
#elif defined(BOARD_FR)
  accel_fr_mailbox.wheel_speed = wheel_speed_rads;
#elif defined(BOARD_RL)
  accel_rl_mailbox.wheel_speed = wheel_speed_rads;
#elif defined(BOARD_RR)
  accel_rr_mailbox.wheel_speed = wheel_speed_rads;
#endif

  taskEXIT_CRITICAL();
}

void usm_can_update_phase(const wheel_phase_output_t *estimate, uint8_t sequence) {
  if (estimate == NULL) return;
  uint32_t now = orion_time_us();
  taskENTER_CRITICAL();
  if (sync_handle != NULL && sync_handle->ever_received &&
      sync_mailbox.version == 1 &&
      (!have_sync || sync_mailbox.sequence != sync_sequence)) {
    /* Software capture includes bus/FIFO latency. Qualification is a measured
     * commissioning decision, not a consequence of receiving a counter. */
    sync_offset = sync_mailbox.vcu_time_us - sync_handle->latest_rx_us;
    sync_received_us = sync_handle->latest_rx_us;
    sync_sequence = sync_mailbox.sequence;
    sync_qualified = sync_mailbox.qualified == 1;
    have_sync = true;
  }
  bool synchronized = have_sync && sync_qualified &&
      ORION_WHEEL_ACQUISITION_TIMING_QUALIFIED &&
      (uint32_t)(now - sync_received_us) < 250000u;
  bool valid = estimate->valid && isfinite(estimate->speed_rad_s) &&
      fabsf(estimate->speed_rad_s) <= 327.67f &&
      (uint32_t)(now - estimate->sample_time_us) < 5000u;
  phase_mailbox.angular_speed = valid ? estimate->speed_rad_s : 0.0f;
  phase_mailbox.estimate_time_us = estimate->sample_time_us +
      (have_sync ? sync_offset : 0u);
  phase_mailbox.sequence = sequence;
  phase_mailbox.status = TC_WIRE_VERSION_1 |
      (valid ? TC_WIRE_VALID | TC_WIRE_DIRECTION : TC_WIRE_FAULT) |
      (synchronized ? TC_WIRE_SYNC : 0u);
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
