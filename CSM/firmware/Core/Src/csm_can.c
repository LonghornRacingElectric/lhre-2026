#include "csm_can.h"
#include "longhorn/can/can_ids.h"

#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "fdcan.h"
#include "longhorn/rtos/can.h"
#include "longhorn/rtos/logger.h"

#include <math.h>
#include <ota_flash.h>
#include <stm32g4xx_hal_fdcan.h>

/* ===============================
   Location-specific defines
   =============================== */

#if defined(BOARD_FL)
    #define CSM_DEVICE_ID       DEVICE_ID_CSM_FL
    #define SUS_ID              FL_STRAIN_GAUGE_SUS_POT_ID
    #define SUS_DLC             FL_STRAIN_GAUGE_SUS_POT_DLC
    #define SUS_FREQ            FL_STRAIN_GAUGE_SUS_POT_FREQ
    #define pack_sus            pack_fl_strain_gauge_sus_pot
    #define msg_sus_t           msg_fl_strain_gauge_sus_pot_t
    #define SUS_STRAIN_FIELD    front_left_strain_gauge_voltage
    #define SUS_POT_FIELD       front_left_suspension_potentiometer
    #define ACCEL_RH_ID         FL_ACCEL_RIDE_HEIGHT_ID
    #define ACCEL_RH_DLC        FL_ACCEL_RIDE_HEIGHT_DLC
    #define ACCEL_RH_FREQ       FL_ACCEL_RIDE_HEIGHT_FREQ
    #define pack_accel_rh       pack_fl_accel_ride_height
    #define msg_accel_rh_t      msg_fl_accel_ride_height_t
    #define GYRO_ID             FL_GYRO_ID
    #define GYRO_DLC            FL_GYRO_DLC
    #define GYRO_FREQ           FL_GYRO_FREQ
    #define pack_gyro           pack_fl_gyro
    #define msg_gyro_t          msg_fl_gyro_t
#elif defined(BOARD_FR)
    #define CSM_DEVICE_ID       DEVICE_ID_CSM_FR
    #define SUS_ID              FR_STRAIN_GAUGE_SUS_POT_ID
    #define SUS_DLC             FR_STRAIN_GAUGE_SUS_POT_DLC
    #define SUS_FREQ            FR_STRAIN_GAUGE_SUS_POT_FREQ
    #define pack_sus            pack_fr_strain_gauge_sus_pot
    #define msg_sus_t           msg_fr_strain_gauge_sus_pot_t
    #define SUS_STRAIN_FIELD    front_right_strain_gauge_voltage
    #define SUS_POT_FIELD       front_right_suspension_potentiometer
    #define ACCEL_RH_ID         FR_ACCEL_RIDE_HEIGHT_ID
    #define ACCEL_RH_DLC        FR_ACCEL_RIDE_HEIGHT_DLC
    #define ACCEL_RH_FREQ       FR_ACCEL_RIDE_HEIGHT_FREQ
    #define pack_accel_rh       pack_fr_accel_ride_height
    #define msg_accel_rh_t      msg_fr_accel_ride_height_t
    #define GYRO_ID             FR_GYRO_ID
    #define GYRO_DLC            FR_GYRO_DLC
    #define GYRO_FREQ           FR_GYRO_FREQ
    #define pack_gyro           pack_fr_gyro
    #define msg_gyro_t          msg_fr_gyro_t
#elif defined(BOARD_RL)
    #define CSM_DEVICE_ID       DEVICE_ID_CSM_RL
    #define SUS_ID              RL_STRAIN_GAUGE_SUS_POT_ID
    #define SUS_DLC             RL_STRAIN_GAUGE_SUS_POT_DLC
    #define SUS_FREQ            RL_STRAIN_GAUGE_SUS_POT_FREQ
    #define pack_sus            pack_rl_strain_gauge_sus_pot
    #define msg_sus_t           msg_rl_strain_gauge_sus_pot_t
    #define SUS_STRAIN_FIELD    back_left_strain_gauge_voltage
    #define SUS_POT_FIELD       back_left_suspension_potentiometer
    #define ACCEL_RH_ID         RL_ACCEL_RIDE_HEIGHT_ID
    #define ACCEL_RH_DLC        RL_ACCEL_RIDE_HEIGHT_DLC
    #define ACCEL_RH_FREQ       RL_ACCEL_RIDE_HEIGHT_FREQ
    #define pack_accel_rh       pack_rl_accel_ride_height
    #define msg_accel_rh_t      msg_rl_accel_ride_height_t
    #define GYRO_ID             RL_GYRO_ID
    #define GYRO_DLC            RL_GYRO_DLC
    #define GYRO_FREQ           RL_GYRO_FREQ
    #define pack_gyro           pack_rl_gyro
    #define msg_gyro_t          msg_rl_gyro_t
#elif defined(BOARD_RR)
    #define CSM_DEVICE_ID       DEVICE_ID_CSM_RR
    #define SUS_ID              RR_STRAIN_GAUGE_SUS_POT_ID
    #define SUS_DLC             RR_STRAIN_GAUGE_SUS_POT_DLC
    #define SUS_FREQ            RR_STRAIN_GAUGE_SUS_POT_FREQ
    #define pack_sus            pack_rr_strain_gauge_sus_pot
    #define msg_sus_t           msg_rr_strain_gauge_sus_pot_t
    #define SUS_STRAIN_FIELD    back_right_strain_gauge_voltage
    #define SUS_POT_FIELD       back_right_suspension_potentiometer
    #define ACCEL_RH_ID         RR_ACCEL_RIDE_HEIGHT_ID
    #define ACCEL_RH_DLC        RR_ACCEL_RIDE_HEIGHT_DLC
    #define ACCEL_RH_FREQ       RR_ACCEL_RIDE_HEIGHT_FREQ
    #define pack_accel_rh       pack_rr_accel_ride_height
    #define msg_accel_rh_t      msg_rr_accel_ride_height_t
    #define GYRO_ID             RR_GYRO_ID
    #define GYRO_DLC            RR_GYRO_DLC
    #define GYRO_FREQ           RR_GYRO_FREQ
    #define pack_gyro           pack_rr_gyro
    #define msg_gyro_t          msg_rr_gyro_t
#else
    #error "No BOARD location defined. Build with a location target e.g. csm_firmware_2026_FL"
#endif

/* ===============================
   CAN Interfaces
   =============================== */

static can_interface_t data_acq_bus;

/* ===============================
   CAN Mailboxes
   =============================== */

static msg_sus_t strain_gauge_sus_pot_mailbox = {0};
static can_message_t *strain_gauge_sus_pot_handle = NULL;

static msg_accel_rh_t accel_ride_height_mailbox = {0};
static can_message_t *accel_ride_height_handle = NULL;

static msg_gyro_t gyro_mailbox = {0};
static can_message_t *gyro_handle = NULL;

/* ===============================
   Internal Function Prototypes
   =============================== */

static void csm_can_add_send_handlers(void);

/* ===============================
   CAN Initialization
   =============================== */

void csm_can_init(void) {
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
        .device_id = CSM_DEVICE_ID,
        .write_memory_fn = ota_flash_write_memory,
        .fw_update_begin_fn = ota_flash_begin,
        .abort_update_fn = ota_flash_abort,
    };

    data_acq_bus.handle = &hfdcan2;
    data_acq_bus.cccr_reg = &hfdcan2.Instance->CCCR;

    can_rtos_init(&cfg);
    can_rtos_register_interface(&data_acq_bus);
    csm_can_add_send_handlers();

    HAL_FDCAN_Start(&hfdcan2);
    can_rtos_start_interface(&data_acq_bus);
    can_rtos_start_transceiver_task(osPriorityHigh);
    can_rtos_start_receiver_task(osPriorityHigh);

    log_printf(LOG_INFO, "[CSM] CAN RTOS initialized\n");
}

/* ===============================
   Send Handler Registration
   =============================== */

static void csm_can_add_send_handlers(void) {
    strain_gauge_sus_pot_handle = can_get_message_handle(
        &strain_gauge_sus_pot_mailbox, SUS_ID, SUS_FREQ, SUS_DLC,
        (CAN_pack_message_fn)pack_sus);
    can_rtos_register_send_packet(&data_acq_bus, strain_gauge_sus_pot_handle);

    accel_ride_height_handle = can_get_message_handle(
        &accel_ride_height_mailbox, ACCEL_RH_ID, ACCEL_RH_FREQ, ACCEL_RH_DLC,
        (CAN_pack_message_fn)pack_accel_rh);
    can_rtos_register_send_packet(&data_acq_bus, accel_ride_height_handle);

    gyro_handle = can_get_message_handle(
        &gyro_mailbox, GYRO_ID, GYRO_FREQ, GYRO_DLC,
        (CAN_pack_message_fn)pack_gyro);
    can_rtos_register_send_packet(&data_acq_bus, gyro_handle);

    log_printf(LOG_INFO, "[CSM] CAN send handlers registered\n");
}

/* ===============================
   Packet Update APIs
   =============================== */

void csm_can_update_strain_gauge_sus_pot(float suspot_travel, float strain_voltage) {
    taskENTER_CRITICAL();
    strain_gauge_sus_pot_mailbox.SUS_STRAIN_FIELD = strain_voltage;
    strain_gauge_sus_pot_mailbox.SUS_POT_FIELD    = suspot_travel;
    taskEXIT_CRITICAL();
}

void csm_can_update_accel_ride_height(float x, float y, float z, float ride_height_mm) {
    taskENTER_CRITICAL();
    accel_ride_height_mailbox.x           = x;
    accel_ride_height_mailbox.y           = y;
    accel_ride_height_mailbox.z           = z;
    accel_ride_height_mailbox.ride_height = ride_height_mm;
    taskEXIT_CRITICAL();
}

void csm_can_update_gyro(float x, float y, float z) {
    taskENTER_CRITICAL();
    gyro_mailbox.x = x;
    gyro_mailbox.y = y;
    gyro_mailbox.z = z;
    taskEXIT_CRITICAL();
}

void csm_can_debug(void) {
    log_printf(LOG_INFO, "[CSM] sent: %lu dropped: %lu err: %d errcode: %d\n",
               data_acq_bus._messages_sent, data_acq_bus.dropped_packets,
               data_acq_bus._error_occurred, data_acq_bus._error_code_send);
}



// #include "csm_can.h"
// #include "longhorn/can/can_ids.h"

// #include "FreeRTOS.h"
// #include "cmsis_os.h"
// #include "fdcan.h"
// #include "longhorn/rtos/can.h"
// #include "longhorn/rtos/logger.h"

// #include <math.h>
// #include <ota_flash.h>
// #include <stm32g4xx_hal_fdcan.h>

// /* ===============================
//    Location-specific defines
//    =============================== */

// #if defined(BOARD_FL)
//     #define CSM_DEVICE_ID       DEVICE_ID_CSM_FL
//     #define SUS_ID              FL_STRAIN_GAUGE_SUS_POT_ID
//     #define SUS_DLC             FL_STRAIN_GAUGE_SUS_POT_DLC
//     #define SUS_FREQ            FL_STRAIN_GAUGE_SUS_POT_FREQ
//     #define pack_sus            pack_fl_strain_gauge_sus_pot
//     #define msg_sus_t           msg_fl_strain_gauge_sus_pot_t
//     #define SUS_STRAIN_FIELD    front_left_strain_gauge_voltage
//     #define SUS_POT_FIELD       front_left_suspension_potentiometer
//     #define ACCEL_RH_ID         FL_ACCEL_RIDE_HEIGHT_ID
//     #define ACCEL_RH_DLC        FL_ACCEL_RIDE_HEIGHT_DLC
//     #define ACCEL_RH_FREQ       FL_ACCEL_RIDE_HEIGHT_FREQ
//     #define pack_accel_rh       pack_fl_accel_ride_height
//     #define msg_accel_rh_t      msg_fl_accel_ride_height_t
// #elif defined(BOARD_FR)
//     #define CSM_DEVICE_ID       DEVICE_ID_CSM_FR
//     #define SUS_ID              FR_STRAIN_GAUGE_SUS_POT_ID
//     #define SUS_DLC             FR_STRAIN_GAUGE_SUS_POT_DLC
//     #define SUS_FREQ            FR_STRAIN_GAUGE_SUS_POT_FREQ
//     #define pack_sus            pack_fr_strain_gauge_sus_pot
//     #define msg_sus_t           msg_fr_strain_gauge_sus_pot_t
//     #define SUS_STRAIN_FIELD    front_right_strain_gauge_voltage
//     #define SUS_POT_FIELD       front_right_suspension_potentiometer
//     #define ACCEL_RH_ID         FR_ACCEL_RIDE_HEIGHT_ID
//     #define ACCEL_RH_DLC        FR_ACCEL_RIDE_HEIGHT_DLC
//     #define ACCEL_RH_FREQ       FR_ACCEL_RIDE_HEIGHT_FREQ
//     #define pack_accel_rh       pack_fr_accel_ride_height
//     #define msg_accel_rh_t      msg_fr_accel_ride_height_t
// #elif defined(BOARD_RL)
//     #define CSM_DEVICE_ID       DEVICE_ID_CSM_RL
//     #define SUS_ID              RL_STRAIN_GAUGE_SUS_POT_ID
//     #define SUS_DLC             RL_STRAIN_GAUGE_SUS_POT_DLC
//     #define SUS_FREQ            RL_STRAIN_GAUGE_SUS_POT_FREQ
//     #define pack_sus            pack_rl_strain_gauge_sus_pot
//     #define msg_sus_t           msg_rl_strain_gauge_sus_pot_t
//     #define SUS_STRAIN_FIELD    back_left_strain_gauge_voltage
//     #define SUS_POT_FIELD       back_left_suspension_potentiometer
//     #define ACCEL_RH_ID         RL_ACCEL_RIDE_HEIGHT_ID
//     #define ACCEL_RH_DLC        RL_ACCEL_RIDE_HEIGHT_DLC
//     #define ACCEL_RH_FREQ       RL_ACCEL_RIDE_HEIGHT_FREQ
//     #define pack_accel_rh       pack_rl_accel_ride_height
//     #define msg_accel_rh_t      msg_rl_accel_ride_height_t
// #elif defined(BOARD_RR)
//     #define CSM_DEVICE_ID       DEVICE_ID_CSM_RR
//     #define SUS_ID              RR_STRAIN_GAUGE_SUS_POT_ID
//     #define SUS_DLC             RR_STRAIN_GAUGE_SUS_POT_DLC
//     #define SUS_FREQ            RR_STRAIN_GAUGE_SUS_POT_FREQ
//     #define pack_sus            pack_rr_strain_gauge_sus_pot
//     #define msg_sus_t           msg_rr_strain_gauge_sus_pot_t
//     #define SUS_STRAIN_FIELD    back_right_strain_gauge_voltage
//     #define SUS_POT_FIELD       back_right_suspension_potentiometer
//     #define ACCEL_RH_ID         RR_ACCEL_RIDE_HEIGHT_ID
//     #define ACCEL_RH_DLC        RR_ACCEL_RIDE_HEIGHT_DLC
//     #define ACCEL_RH_FREQ       RR_ACCEL_RIDE_HEIGHT_FREQ
//     #define pack_accel_rh       pack_rr_accel_ride_height
//     #define msg_accel_rh_t      msg_rr_accel_ride_height_t
// #else
//     #error "No BOARD location defined. Build with a location target e.g. csm_firmware_2026_FL"
// #endif

// /* ===============================
//    CAN Interfaces
//    =============================== */

// static can_interface_t data_acq_bus;

// /* ===============================
//    CAN Mailboxes
//    =============================== */

// static msg_sus_t strain_gauge_sus_pot_mailbox = {0};
// static can_message_t *strain_gauge_sus_pot_handle = NULL;

// static msg_accel_rh_t accel_ride_height_mailbox = {0};
// static can_message_t *accel_ride_height_handle = NULL;

// /* ===============================
//    Internal Function Prototypes
//    =============================== */

// static void csm_can_add_send_handlers(void);

// /* ===============================
//    CAN Initialization
//    =============================== */

// void csm_can_init(void) {
//     ota_flash_init();

//     can_config_t cfg = {
//         .init_fn = (CAN_Init_fn)HAL_FDCAN_Init,
//         .start_fn = (CAN_Start_fn)HAL_FDCAN_Start,
//         .noti_fn = (CAN_ActivateNotifications_fn)HAL_FDCAN_ActivateNotification,
//         .stop_fn = (CAN_Stop_fn)HAL_FDCAN_Stop,
//         .add_to_queue_fn = (CAN_AddToQ_fn)HAL_FDCAN_AddMessageToTxFifoQ,
//         .get_tx_fifo_free_level_fn =
//             (CAN_GetTxFifoFreeLevel_fn)HAL_FDCAN_GetTxFifoFreeLevel,
//         .get_rx_message_fn = (CAN_GetRxMessage_fn)HAL_FDCAN_GetRxMessage,
//         .get_rx_fifo_fill_level_fn =
//             (CAN_GetRxFifoFillLevel_fn)HAL_FDCAN_GetRxFifoFillLevel,
//         .tick_fn = HAL_GetTick,
//         .add_filter_fn = (CAN_AddFilter_fn)HAL_FDCAN_ConfigFilter,
//         .malloc_fn = pvPortMalloc,
//         .free_fn = vPortFree,
//         .init_bit = FDCAN_CCCR_INIT,
//         .device_id = CSM_DEVICE_ID,
//         .write_memory_fn = ota_flash_write_memory,
//         .fw_update_begin_fn = ota_flash_begin,
//         .abort_update_fn = ota_flash_abort,
//     };

//     data_acq_bus.handle = &hfdcan2;
//     data_acq_bus.cccr_reg = &hfdcan2.Instance->CCCR;

//     can_rtos_init(&cfg);
//     can_rtos_register_interface(&data_acq_bus);
//     csm_can_add_send_handlers();

//     HAL_FDCAN_Start(&hfdcan2);
//     can_rtos_start_interface(&data_acq_bus);
//     can_rtos_start_transceiver_task(osPriorityHigh);
//     can_rtos_start_receiver_task(osPriorityHigh);

//     log_printf(LOG_INFO, "[CSM] CAN RTOS initialized\n");
// }

// /* ===============================
//    Send Handler Registration
//    =============================== */

// static void csm_can_add_send_handlers(void) {
//     strain_gauge_sus_pot_handle = can_get_message_handle(
//         &strain_gauge_sus_pot_mailbox, SUS_ID, SUS_FREQ, SUS_DLC,
//         (CAN_pack_message_fn)pack_sus);
//     can_rtos_register_send_packet(&data_acq_bus, strain_gauge_sus_pot_handle);

//     accel_ride_height_handle = can_get_message_handle(
//         &accel_ride_height_mailbox, ACCEL_RH_ID, ACCEL_RH_FREQ, ACCEL_RH_DLC,
//         (CAN_pack_message_fn)pack_accel_rh);
//     can_rtos_register_send_packet(&data_acq_bus, accel_ride_height_handle);

//     log_printf(LOG_INFO, "[CSM] CAN send handlers registered\n");
// }

// /* ===============================
//    Packet Update APIs
//    =============================== */

// void csm_can_update_strain_gauge_sus_pot(float suspot_travel, float strain_voltage) {
//     taskENTER_CRITICAL();
//     strain_gauge_sus_pot_mailbox.SUS_STRAIN_FIELD = strain_voltage;
//     strain_gauge_sus_pot_mailbox.SUS_POT_FIELD    = suspot_travel;
//     taskEXIT_CRITICAL();
// }

// void csm_can_update_accel_ride_height(float x, float y, float z, float ride_height_mm) {
//     taskENTER_CRITICAL();
//     accel_ride_height_mailbox.x            = x;
//     accel_ride_height_mailbox.y            = y;
//     accel_ride_height_mailbox.z            = z;
//     accel_ride_height_mailbox.ride_height  = ride_height_mm;
//     taskEXIT_CRITICAL();
// }


// void csm_can_debug(void) {
//     log_printf(LOG_INFO, "[CSM] sent: %lu dropped: %lu err: %d errcode: %d\n",
//                data_acq_bus._messages_sent, data_acq_bus.dropped_packets,
//                data_acq_bus._error_occurred, data_acq_bus._error_code_send);
// }
