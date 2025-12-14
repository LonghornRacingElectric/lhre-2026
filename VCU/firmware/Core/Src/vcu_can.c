#include "vcu_can.h"

#include "FreeRTOS.h"
#include "fdcan.h"

#include "longhorn/rtos/can.h"
#include "longhorn/rtos/logger.h"
#include "longhorn/can/can_ids.h"

#include <string.h>

extern FDCAN_HandleTypeDef hfdcan1;

// 0x0C0  (VCU → Inverter)  TORQUE COMMAND
// Byte 0–1 : torque_request   (int16, 0.1 Nm units)
// Byte 2–3 : rpm_request      (int16, rpm)
// Byte 4   : direction        (uint8, 0=fwd, 1=rev)
// Byte 5   : enable           (uint8, 0=disable, 1=enable)
// Byte 6–7 : torque_limit     (int16, 0.1 Nm)

// 0x0B0  (Inverter → VCU)  SPEED / TORQUE FEEDBACK
// Byte 0–1 : torque_command   (int16, 0.1 Nm)
// Byte 2–3 : torque_feedback  (int16, 0.1 Nm)
// Byte 4–5 : motor_speed      (int16, rpm)
// Byte 6–7 : bus_voltage      (uint16, 0.1 V)

// NOTES:
// direction & enable are boolean in meaning, but MUST be uint8 in CAN
// Classic CAN only
// CM200 requires periodic torque command (~333 Hz).
// Scaling:
//  *        torque_Nm   → int16 = Nm * 10
//  *        bus_voltage → raw * 0.1
//  *        torque_fb   → raw * 0.1

static can_interface_t can1 = {
    .handle = &hfdcan1,
};

// TX: torque command (0x0C0)
static msg_inverter_torque_command_t inv_tx;
static can_message_t *inv_tx_handle;

// RX: inverter speed feedback (0x0B0)
static msg_inverter_speed_t inv_speed;
static can_receive_message_t *inv_speed_rx;

// Public feedback values
float   inverter_torque_fb   = 0.0f;
int16_t inverter_rpm         = 0;
float   inverter_bus_voltage = 0.0f;

// RTOS CAN configuration 
static can_config_t cfg = {
    .init_fn           = (CAN_Init_fn)HAL_FDCAN_Init,
    .start_fn          = (CAN_Start_fn)HAL_FDCAN_Start,
    .noti_fn           = (CAN_ActivateNotifications_fn)HAL_FDCAN_ActivateNotification,
    .stop_fn           = (CAN_Stop_fn)HAL_FDCAN_Stop,
    .add_to_queue_fn   = (CAN_AddToQ_fn)HAL_FDCAN_AddMessageToTxFifoQ,
    .get_rx_message_fn = (CAN_GetRxMessage_fn)HAL_FDCAN_GetRxMessage,
    .tick_fn           = HAL_GetTick,
    .add_filter_fn     = (CAN_AddFilter_fn)HAL_FDCAN_ConfigFilter,
    .malloc_fn         = pvPortMalloc,
    .free_fn           = vPortFree,
};

// Initialization 
void vcu_can_init(void)
{
    // Initialize RTOS CAN core
    can_rtos_init(&cfg);

    // Start CAN RTOS tasks ONCE
    can_rtos_start_transceiver_task(configMAX_PRIORITIES - 2);
    can_rtos_start_receiver_task(configMAX_PRIORITIES - 2);


    // Register physical interface
    can_rtos_register_interface(&can1);

    // TX: Torque command (0x0C0)
    memset(&inv_tx, 0, sizeof(inv_tx));
    inv_tx.direction    = 1;     // forward
    inv_tx.enable       = 1;     // enable inverter
    inv_tx.torque_limit = 500;   // 50.0 Nm (0.1 Nm units)

    inv_tx_handle = can_get_message_handle(
        &inv_tx,
        INVERTER_TORQUE_COMMAND_ID,
        INVERTER_TORQUE_COMMAND_FREQ,
        INVERTER_TORQUE_COMMAND_DLC,
        (CAN_pack_message_fn)pack_inverter_torque_command
    );

    can_rtos_register_send_packet(&can1, inv_tx_handle);

    // RX: Speed / torque feedback (0x0B0)
    inv_speed_rx = can_get_receive_message_handle(
        &inv_speed,
        INVERTER_SPEED_ID,
        (CAN_unpack_message_fn)unpack_inverter_speed
    );

    can_rtos_register_receive_packet(&can1, inv_speed_rx);

    log_printf(LOG_INFO, "[VCU] CAN RTOS initialized\n");
}

// Torque command update
void vcu_can_set_torque(float torque_nm)
{
    inv_tx.torque_request = (int16_t)(torque_nm * 10.0f);

    // RX data is updated in receiver task
    inverter_torque_fb   = inv_speed.torque_feedback * 0.1f;
    inverter_rpm         = inv_speed.motor_speed;
    inverter_bus_voltage = inv_speed.bus_voltage * 0.1f;
}