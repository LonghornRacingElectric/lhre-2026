#include "can.h"
#include "fdcan.h"
#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "tim.h"
#include <stdio.h>
#include "longhorn/rtos/can.h"
#include "longhorn/rtos/logger.h"
#include "longhorn/can/can_ids.h"
#include "task.h"

/* USER CODE BEGIN 0 */

static msg_contactor_status_t contactor_status_tx;

// static msg_vcu_current_sense_t vcu_current_tx;

static can_interface_t can1 = {
    .handle = &hfdcan1,
};

static can_config_t can_config = {
    .init_fn            = (CAN_Init_fn)HAL_FDCAN_Init,
    .start_fn           = (CAN_Start_fn)HAL_FDCAN_Start,
    .noti_fn            = (CAN_ActivateNotifications_fn)HAL_FDCAN_ActivateNotification,
    .stop_fn            = (CAN_Stop_fn)HAL_FDCAN_Stop,
    .add_to_queue_fn    = (CAN_AddToQ_fn)HAL_FDCAN_AddMessageToTxFifoQ,
    .get_rx_message_fn  = (CAN_GetRxMessage_fn)HAL_FDCAN_GetRxMessage,
    .tick_fn            = (Tick_fn)osKernelGetTickCount,
    .add_filter_fn      = (CAN_AddFilter_fn)HAL_FDCAN_ConfigFilter,
    .malloc_fn          = (Malloc_fn)pvPortMalloc,
    .free_fn            = (Free_fn)vPortFree,
};


void hvc_can_init(void) {

    can_rtos_init(&can_config);
    can_rtos_register_interface(&can1);

    can_register_interface(&can1);

    
    // static msg_vcu_current_sense_t msg_content;
    // can_message_t* msg = can_get_message_handle(
    //     &msg_content, VCU_CURRENT_SENSE_ID, VCU_CURRENT_SENSE_FREQ,
    //     VCU_CURRENT_SENSE_DLC, pack_vcu_current_sense);
    // can_register_send_packet(&can1, msg);

    can_service(&can1);

    can_message_t* contactor_status_handle = can_get_message_handle(
        &contactor_status_tx, CONTACTOR_STATUS_ID, CONTACTOR_STATUS_FREQ,
        CONTACTOR_STATUS_DLC, (CAN_pack_message_fn)pack_contactor_status);

    can_rtos_register_send_packet(&can1, contactor_status_handle);

    can_rtos_start_transceiver_task(osPriorityNormal);
    can_rtos_start_receiver_task(osPriorityAboveNormal);

    log_printf(LOG_INFO, "[HVC] CAN RTOS initialized\n");
}

void hvc_set_contactor_status(int state, bool pos, bool neg) {
    taskENTER_CRITICAL();

    contactor_status_tx.hvc_state_machine = (uint8_t) state;
    contactor_status_tx.positive_hv_contactor = pos ? 1 : 0;
    contactor_status_tx.negative_hv_contactor = neg ? 1 : 0;

    taskEXIT_CRITICAL();
}

/* USER CODE END 0 */