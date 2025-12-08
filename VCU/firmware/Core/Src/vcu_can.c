#include "vcu_can.h"

#include "fdcan.h"
#include "longhorn/can_base.h"
#include "longhorn/can/can_ids.h"
#include "longhorn/rtos/logger.h"

#include <stdlib.h>
#include <string.h>

extern FDCAN_HandleTypeDef hfdcan1;

// CAN interface
static can_interface_t can1;

// TX: torque command (0x0C0)
static msg_inverter_torque_command_t inv_tx;
static can_message_t *inv_tx_handle;

// RX: inverter speed feedback (0x0B0)
static msg_inverter_speed_t inv_speed;
static can_receive_message_t *inv_speed_rx;

float inverter_torque_fb = 0.0f;
int16_t inverter_rpm = 0;
float inverter_bus_voltage = 0.0f;

void vcu_can_init(void)
{
    can_config_t cfg = {
        .init_fn           = HAL_FDCAN_Init,
        .start_fn          = HAL_FDCAN_Start,
        .noti_fn           = HAL_FDCAN_ActivateNotification,
        .stop_fn           = HAL_FDCAN_Stop,
        .add_to_queue_fn   = HAL_FDCAN_AddMessageToTxFifoQ,
        .get_rx_message_fn = HAL_FDCAN_GetRxMessage,
        .tick_fn           = HAL_GetTick,
        .add_filter_fn     = HAL_FDCAN_ConfigFilter,
        .malloc_fn         = malloc,
        .free_fn           = free,
    };

    can_init(&cfg);

    memset(&can1, 0, sizeof(can1));
    can1.handle = &hfdcan1;
    can_register_interface(&can1);

    // Setup torque command (0x0C0)
    inv_tx.torque_request = 0;
    inv_tx.rpm_request = 0;
    inv_tx.direction = 1;
    inv_tx.enable = 1;
    inv_tx.torque_limit = 500;   // 50 Nm (0.1 Nm units)

    inv_tx_handle = can_get_message_handle(
        &inv_tx,
        INVERTER_TORQUE_COMMAND_ID,
        INVERTER_TORQUE_COMMAND_FREQ,
        INVERTER_TORQUE_COMMAND_DLC,
        pack_inverter_torque_command
    );
    can_register_send_packet(&can1, inv_tx_handle);

    // Setup inverter speed feedback (0x0B0)
    inv_speed_rx = can_get_receive_message_handle(
        &inv_speed,
        INVERTER_SPEED_ID,
        unpack_inverter_speed
    );
    can_register_receive_packet(&can1, inv_speed_rx);

    log_printf(LOG_INFO, "[VCU] CAN initialized.\n");
}

void vcu_can_service(void)
{
    can_service(&can1);

    inverter_torque_fb   = inv_speed.torque_feedback * 0.1f;
    inverter_rpm         = inv_speed.motor_speed;
    inverter_bus_voltage = inv_speed.bus_voltage * 0.1f;
}

void vcu_can_set_torque(float torque_nm)
{
    inv_tx.torque_request = (int16_t)(torque_nm * 10.0f);
}
