#include "usm_can.h"

#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "main.h"
#include "longhorn/rtos/can.h"
#include "ota/ota_flash.h"
#include "stm32g4xx_hal_fdcan.h"

/* ===============================
   CAN Interface
   =============================== */

static can_interface_t data_acq_bus;

/* ===============================
   CAN Mailboxes
   =============================== */

/* 0x130: Wheel Speed + Ride Height → VCU (10Hz, DLC 4) */
static msg_wheel_speed_ride_height_t vcu_mailbox = {0};
static can_message_t *vcu_handle = NULL;

/* 0x400: All 4 Wheel Speeds → Pi (100Hz, DLC 8) */
static msg_wheel_speeds_t pi_mailbox = {0};
static can_message_t *pi_handle = NULL;

/* 0x402-0x405: Unsprung Acceleration → Pi (100Hz, DLC 6), corner-specific */
#if defined(BOARD_FL)
static msg_acceleration_vector_unsprung_fl_t accel_mailbox = {0};
static can_message_t *accel_handle = NULL;
#elif defined(BOARD_FR)
static msg_acceleration_vector_unsprung_fr_t accel_mailbox = {0};
static can_message_t *accel_handle = NULL;
#elif defined(BOARD_RL)
static msg_acceleration_vector_unsprung_rl_t accel_mailbox = {0};
static can_message_t *accel_handle = NULL;
#elif defined(BOARD_RR)
static msg_acceleration_vector_unsprung_rr_t accel_mailbox = {0};
static can_message_t *accel_handle = NULL;
#endif

/* ===============================
   Internal Prototypes
   =============================== */

static void usm_can_add_send_handlers(void);

/* ===============================
   Init
   =============================== */

void usm_can_init(void) {
    ota_flash_init();

    can_config_t cfg = {
        .init_fn                    = (CAN_Init_fn)HAL_FDCAN_Init,
        .start_fn                   = (CAN_Start_fn)HAL_FDCAN_Start,
        .noti_fn                    = (CAN_ActivateNotifications_fn)HAL_FDCAN_ActivateNotification,
        .stop_fn                    = (CAN_Stop_fn)HAL_FDCAN_Stop,
        .add_to_queue_fn            = (CAN_AddToQ_fn)HAL_FDCAN_AddMessageToTxFifoQ,
        .get_tx_fifo_free_level_fn  = (CAN_GetTxFifoFreeLevel_fn)HAL_FDCAN_GetTxFifoFreeLevel,
        .get_rx_message_fn          = (CAN_GetRxMessage_fn)HAL_FDCAN_GetRxMessage,
        .get_rx_fifo_fill_level_fn  = (CAN_GetRxFifoFillLevel_fn)HAL_FDCAN_GetRxFifoFillLevel,
        .tick_fn                    = HAL_GetTick,
        .add_filter_fn              = (CAN_AddFilter_fn)HAL_FDCAN_ConfigFilter,
        .malloc_fn                  = pvPortMalloc,
        .free_fn                    = vPortFree,
        .init_bit                   = FDCAN_CCCR_INIT,
#if defined(BOARD_FL)
        .device_id                  = DEVICE_ID_USM_FL,
#elif defined(BOARD_FR)
        .device_id                  = DEVICE_ID_USM_FR,
#elif defined(BOARD_RL)
        .device_id                  = DEVICE_ID_USM_RL,
#elif defined(BOARD_RR)
        .device_id                  = DEVICE_ID_USM_RR,
#else
        .device_id                  = DEVICE_ID_USM_FR,
#endif
        .write_memory_fn            = ota_flash_write_memory,
        .fw_update_begin_fn         = ota_flash_begin,
        .abort_update_fn            = ota_flash_abort,
    };

    extern FDCAN_HandleTypeDef hfdcan2;
    data_acq_bus.handle   = &hfdcan2;
    data_acq_bus.cccr_reg = &hfdcan2.Instance->CCCR;

    can_rtos_init(&cfg);
    can_rtos_register_interface(&data_acq_bus);

    usm_can_add_send_handlers();

    HAL_FDCAN_Start(&hfdcan2);
    can_rtos_start_interface(&data_acq_bus);

    can_rtos_start_transceiver_task(osPriorityHigh);
    can_rtos_start_receiver_task(osPriorityHigh);
}

/* ===============================
   Send Handler Registration
   =============================== */

static void usm_can_add_send_handlers(void) {
    /* 0x130 → VCU: wheel speed + ride height, 10Hz */
    vcu_handle = can_get_message_handle(
        &vcu_mailbox,
        WHEEL_SPEED_RIDE_HEIGHT_ID,
        WHEEL_SPEED_RIDE_HEIGHT_FREQ,
        WHEEL_SPEED_RIDE_HEIGHT_DLC,
        (CAN_pack_message_fn)pack_wheel_speed_ride_height);
    can_rtos_register_send_packet(&data_acq_bus, vcu_handle);

    /* 0x400 → Pi: all 4 wheel speeds, 100Hz */
    pi_handle = can_get_message_handle(
        &pi_mailbox,
        WHEEL_SPEEDS_ID,
        WHEEL_SPEEDS_FREQ,
        WHEEL_SPEEDS_DLC,
        (CAN_pack_message_fn)pack_wheel_speeds);
    can_rtos_register_send_packet(&data_acq_bus, pi_handle);

    /* 0x402-0x405 → Pi: unsprung acceleration, 100Hz */
#if defined(BOARD_FL)
    accel_handle = can_get_message_handle(
        &accel_mailbox,
        ACCELERATION_VECTOR_UNSPRUNG_FL_ID,
        ACCELERATION_VECTOR_UNSPRUNG_FL_FREQ,
        ACCELERATION_VECTOR_UNSPRUNG_FL_DLC,
        (CAN_pack_message_fn)pack_acceleration_vector_unsprung_fl);
#elif defined(BOARD_FR)
    accel_handle = can_get_message_handle(
        &accel_mailbox,
        ACCELERATION_VECTOR_UNSPRUNG_FR_ID,
        ACCELERATION_VECTOR_UNSPRUNG_FR_FREQ,
        ACCELERATION_VECTOR_UNSPRUNG_FR_DLC,
        (CAN_pack_message_fn)pack_acceleration_vector_unsprung_fr);
#elif defined(BOARD_RL)
    accel_handle = can_get_message_handle(
        &accel_mailbox,
        ACCELERATION_VECTOR_UNSPRUNG_RL_ID,
        ACCELERATION_VECTOR_UNSPRUNG_RL_FREQ,
        ACCELERATION_VECTOR_UNSPRUNG_RL_DLC,
        (CAN_pack_message_fn)pack_acceleration_vector_unsprung_rl);
#elif defined(BOARD_RR)
    accel_handle = can_get_message_handle(
        &accel_mailbox,
        ACCELERATION_VECTOR_UNSPRUNG_RR_ID,
        ACCELERATION_VECTOR_UNSPRUNG_RR_FREQ,
        ACCELERATION_VECTOR_UNSPRUNG_RR_DLC,
        (CAN_pack_message_fn)pack_acceleration_vector_unsprung_rr);
#endif
    can_rtos_register_send_packet(&data_acq_bus, accel_handle);
}

/* ===============================
   Packet Update API
   =============================== */

void usm_can_update_accel(float ax, float ay, float az) {
    taskENTER_CRITICAL();
    accel_mailbox.x = ax;
    accel_mailbox.y = ay;
    accel_mailbox.z = az;
    taskEXIT_CRITICAL();
}

void usm_can_update_wheel_speed(float wheel_speed_rads) {
    taskENTER_CRITICAL();

    /* 0x130 → VCU */
    vcu_mailbox.wheel_speed = wheel_speed_rads;
    vcu_mailbox.ride_height = 0.0f;  // ride height not implemented on USM

    /* 0x400 → Pi: fill only the field for this corner based on board define */
#if defined(BOARD_FL)
    pi_mailbox.front_left_speed = wheel_speed_rads;
#elif defined(BOARD_FR)
    pi_mailbox.front_right_speed = wheel_speed_rads;
#elif defined(BOARD_RL)
    pi_mailbox.back_left_speed = wheel_speed_rads;
#elif defined(BOARD_RR)
    pi_mailbox.back_right_speed = wheel_speed_rads;
#endif

    taskEXIT_CRITICAL();
}
