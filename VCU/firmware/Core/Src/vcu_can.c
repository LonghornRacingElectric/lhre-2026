#include "vcu_can.h"
#include "FreeRTOS.h"
#include "fdcan.h"
#include "longhorn/rtos/can.h"
#include "longhorn/rtos/logger.h"
#include "longhorn/can/can_ids.h"
#include "task.h"

#include <string.h>

#define TABLE_SPIN_TORQUE_LIMIT_NM 10.0f
extern FDCAN_HandleTypeDef hfdcan1;

// 0x0C0  (VCU → Inverter)  TORQUE COMMAND
// Byte 0–1 : torque_request   (int16, 0.1 Nm units)
// Byte 2–3 : rpm_request      (int16, rpm)
// Byte 4   : direction        (uint8, 1=fwd, 0=rev)
// Byte 5   : enable           (uint8, 0=disable, 1=enable)
// Byte 6–7 : torque_limit     (int16, 0.1 Nm)

// 0x0B0  (Inverter → VCU)  SPEED / TORQUE FEEDBACK
// Byte 0–1 : torque_command   (int16, 0.1 Nm)
// Byte 2–3 : torque_feedback  (int16, 0.1 Nm)
// Byte 4–5 : motor_speed      (int16, rpm)
// Byte 6–7 : bus_voltage      (uint16, 0.1 V)

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

void vcu_can_init(void)
{
    can_rtos_init(&cfg);

    // Register physical interface FIRST
    can_rtos_register_interface(&can1);

    // TX handle
    memset(&inv_tx, 0, sizeof(inv_tx));
    inv_tx.direction    = 1;      
    inv_tx.enable       = 1;
    inv_tx.rpm_request  = 0;      // explicit
    inv_tx.torque_limit = TABLE_SPIN_TORQUE_LIMIT_NM;

    inv_tx_handle = can_get_message_handle(
        &inv_tx,
        INVERTER_TORQUE_COMMAND_ID,
        INVERTER_TORQUE_COMMAND_FREQ,
        INVERTER_TORQUE_COMMAND_DLC,
        (CAN_pack_message_fn)pack_inverter_torque_command
    );
    can_rtos_register_send_packet(&can1, inv_tx_handle);

    // RX handle 
    inv_speed_rx = can_get_receive_message_handle(
        &inv_speed,
        INVERTER_SPEED_ID,
        (CAN_unpack_message_fn)unpack_inverter_speed
    );
    can_rtos_register_receive_packet(&can1, inv_speed_rx);

    // Start tasks LAST (once everything is registered)
    can_rtos_start_transceiver_task(configMAX_PRIORITIES - 2);
    can_rtos_start_receiver_task(configMAX_PRIORITIES - 2);

    log_printf(LOG_INFO, "[VCU] CAN RTOS initialized\n");
}



void vcu_can_set_torque(float torque_nm)
{
    taskENTER_CRITICAL();

    // No negative torque for table spin
    if (torque_nm < 0.0f) {
        torque_nm = 0.0f;
    }

    // HARD safety clamp
    if (torque_nm > TABLE_SPIN_TORQUE_LIMIT_NM) {
        torque_nm = TABLE_SPIN_TORQUE_LIMIT_NM;
    }

    inv_tx.torque_request = torque_nm;
    inv_tx.torque_limit   = TABLE_SPIN_TORQUE_LIMIT_NM;

    taskEXIT_CRITICAL();
}



void vcu_can_read_feedback(void)
{
    msg_inverter_speed_t local;

    taskENTER_CRITICAL();
    local = inv_speed;
    taskEXIT_CRITICAL();

inverter_torque_fb   = local.torque_feedback * 0.1f; // can_ids.c doesn't scale?
inverter_bus_voltage = local.bus_voltage     * 0.1f; // can_ids.c doesn't scale?
inverter_rpm         = local.motor_speed;            


}

