#include "vcu_can.h"

#include <stdlib.h>

#include "fdcan.h"
#include "inverter_cm200.h"
#include "longhorn/can_base.h"

// External FDCAN handle from fdcan.c
extern FDCAN_HandleTypeDef hfdcan1;

void vcu_can_init(void)
{
    can_config_t cfg = {
        .init_fn           = (CAN_Init_fn)HAL_FDCAN_Init,
        .start_fn          = (CAN_Start_fn)HAL_FDCAN_Start,
        .noti_fn           = (CAN_ActivateNotifications_fn)HAL_FDCAN_ActivateNotification,
        .stop_fn           = (CAN_Stop_fn)HAL_FDCAN_Stop,
        .add_to_queue_fn   = (CAN_AddToQ_fn)HAL_FDCAN_AddMessageToTxFifoQ,
        .get_rx_message_fn = (CAN_GetRxMessage_fn)HAL_FDCAN_GetRxMessage,
        .tick_fn           = HAL_GetTick,
        .add_filter_fn     = (CAN_AddFilter_fn)HAL_FDCAN_ConfigFilter,
        .malloc_fn         = malloc,
        .free_fn           = free,
    };

    can_init(&cfg);

    cm200_init(&hfdcan1);
}

void vcu_can_service(void)
{
    cm200_process_rx();
}

void vcu_can_set_torque(float torque_nm,
                        float torque_limit_nm,
                        bool enable,
                        uint8_t direction)
{
    (void)torque_limit_nm;
    (void)direction;

    if (!enable) {
        cm200_send_torque(0.0f);
    } else {
        cm200_send_torque(torque_nm);
    }
}

void vcu_can_set_brake_light(bool brake_active)
{
    (void)brake_active;
    // TODO: implement PDU / brake light CAN when mapping is available
}
