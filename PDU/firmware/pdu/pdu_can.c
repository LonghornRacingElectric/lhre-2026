#include "pdu_can.h"

#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "fdcan.h"
#include "longhorn/rtos/can.h"
#include "longhorn/rtos/logger.h"

/** ==
 *  CAN Interface and Configuration Setup
 *  ==
 */

static can_interface_t critical_bus = {
    .handle = &hfdcan1,
};

static can_interface_t data_acq_bus = {
    .handle = &hfdcan2,
};

static can_config_t pdu_can_config = {
    .init_fn = (CAN_Init_fn)HAL_FDCAN_Init,
    .start_fn = (CAN_Start_fn)HAL_FDCAN_Start,
    .noti_fn = (CAN_ActivateNotifications_fn)HAL_FDCAN_ActivateNotification,
    .stop_fn = (CAN_Stop_fn)HAL_FDCAN_Stop,
    .add_to_queue_fn = (CAN_AddToQ_fn)HAL_FDCAN_AddMessageToTxFifoQ,
    .get_rx_message_fn = (CAN_GetRxMessage_fn)HAL_FDCAN_GetRxMessage,
    .tick_fn = HAL_GetTick,
    .add_filter_fn = (CAN_AddFilter_fn)HAL_FDCAN_ConfigFilter,
    .malloc_fn = pvPortMalloc,
    .free_fn = vPortFree,
};

/** ==
 * CAN Packets
 *  ==
 */

static msg_indicators_shutdown_status_t indicator_status_mailbox = {0};
static can_receive_message_t* indicator_status_mailbox_handle = NULL;

static msg_dui_r2d_authorization_t r2d_authorization_mailbox = {0};
static can_receive_message_t* r2d_authorization_mailbox_handle = NULL;

void pdu_can_add_receive_handlers(void);

/**
 * @brief Initializes the CAN interface with the RTOS library and registers
 * handlers. Also starts the CAN transceiver and receiver tasks.
 */
void pdu_can_init(void) {
    can_rtos_init(&pdu_can_config);

    // Register physical interfaces FIRST
    can_rtos_register_interface(&critical_bus);
    can_rtos_register_interface(&data_acq_bus);

    pdu_can_add_receive_handlers();

    can_rtos_start_transceiver_task(osPriorityNormal);
    can_rtos_start_receiver_task(osPriorityAboveNormal);

    log_printf(LOG_INFO, "[PDU] CAN RTOS initialized\n");
}

/**
 * @brief Creates the CAN receive handlers and registers them with the CAN lib
 *
 * Forward declaration is above, but this API is NOT public, so don't call in
 * other files
 *
 */
void pdu_can_add_receive_handlers(void) {
    // Indicators + Shutdown Status
    indicator_status_mailbox_handle = can_get_receive_message_handle(
        &indicator_status_mailbox, INDICATORS_SHUTDOWN_STATUS_ID,
        (CAN_unpack_message_fn)unpack_indicators_shutdown_status);

    can_rtos_register_receive_packet(&critical_bus,
                                     indicator_status_mailbox_handle);

    log_printf(LOG_INFO, "[PDU] CAN IMD + BMS handlers registered\n");

    // DUI R2D Authorization
    r2d_authorization_mailbox_handle = can_get_receive_message_handle(
        &r2d_authorization_mailbox, DUI_R2D_AUTHORIZATION_ID,
        (CAN_unpack_message_fn)unpack_dui_r2d_authorization);

    can_rtos_register_receive_packet(&critical_bus,
                                     r2d_authorization_mailbox_handle);

    log_printf(LOG_INFO, "[PDU] CAN R2D Authorization handler registered\n");
}

bool vehicle_in_park(void) {
    // 0 indicates VCU has NOT authorized R2D, so vehicle is in Park.
    return r2d_authorization_mailbox.r2d_authorized == 0;
}

bool vehicle_in_drive(void) { return !vehicle_in_park(); }

/**
 * @brief Check if there are faults. Checks the mailbox that is updated by the
 * CAN RTOS tasks. Then, returns true if there is a fault present OR if there
 * was a timeout.
 * TODO: implement TIMEOUTS
 * @return true
 * @return false
 */
bool hvc_imd_fault(void) {
    return indicator_status_mailbox.imd_error ||
           message_timed_out(indicator_status_mailbox_handle, 1000);
}
bool hvc_bms_fault(void) {
    return indicator_status_mailbox.bms_error ||
           message_timed_out(indicator_status_mailbox_handle,
                             INDICATORS_SHUTDOWN_STATUS_TIMEOUT_MS);
}