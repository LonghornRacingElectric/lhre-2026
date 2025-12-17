#include "can.h"
#include "fdcan.h"
#include "longhorn/can_base.h"
#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "tim.h"
#include <stdio.h>

/* USER CODE BEGIN 0 */
void can_func(){
static can_config_t can_config = {
        .init_fn = (CAN_Init_fn)HAL_FDCAN_Init,
        .start_fn = (CAN_Start_fn)HAL_FDCAN_Start,
        .noti_fn = (CAN_ActivateNotifications_fn)HAL_FDCAN_ActivateNotification,
        .stop_fn = (CAN_Stop_fn)HAL_FDCAN_Stop,
        .add_to_queue_fn = (CAN_AddToQ_fn)HAL_FDCAN_AddMessageToTxFifoQ,
        .get_rx_message_fn = (CAN_GetRxMessage_fn)HAL_FDCAN_GetRxMessage,
        .tick_fn = (Tick_fn)osKernelGetTickCount,
        .add_filter_fn = (CAN_AddFilter_fn)HAL_FDCAN_ConfigFilter,
        .malloc_fn = (Malloc_fn)pvPortMalloc,
        .free_fn = (Free_fn)vPortFree,
    };

    can_init(&can_config);

    static can_interface_t can_interface = {
        .handle = &hfdcan1,
    };

    can_register_interface(&can_interface);

    static msg_vcu_current_sense_t msg_content;
    can_message_t* msg = can_get_message_handle(
        &msg_content, VCU_CURRENT_SENSE_ID, VCU_CURRENT_SENSE_FREQ,
        VCU_CURRENT_SENSE_DLC, pack_vcu_current_sense);
    can_register_send_packet(&can_interface, msg);

    can_service(&can_interface);

    static msg_contactor_status_t msg2_content;
    can_message_t* msg2 = can_get_message_handle(
        &msg2_content, CONTACTOR_STATUS_ID, CONTACTOR_STATUS_FREQ,
        CONTACTOR_STATUS_DLC, pack_contactor_status);
    can_register_send_packet(&can_interface, msg2);
    
    msg_content.lv_boards_current = 1.0f;
}
/* USER CODE END 0 */