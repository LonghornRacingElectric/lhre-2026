#include "hvc_can.h"

#include <stddef.h>
#include <stdint.h>

#include "cells.h"
#include "contactors.h"
#include "fdcan.h"
#include "gpio.h"
#include "longhorn/can/can_ids.h"
#include "longhorn/can_base.h"
#include "soc_estimation.h"
#include "state_machine.h"
#include "vct_sense.h"

#define CELL_VOLTAGE_PACKET_COUNT 35U
#define CELL_TEMPERATURE_PACKET_COUNT 23U
#define CAN_MESSAGE_POOL_SIZE 4096U
/* The VCU declares 0x131 stale after 20 ms. Send it at 200 Hz so a single
 * delayed service pass does not immediately consume the entire margin. */
#define HVC_CONTACTOR_STATUS_PERIOD_MS 5U
#ifndef HVC_CAN_FORCE_ENERGIZED
#define HVC_CAN_FORCE_ENERGIZED 0
#endif

static msg_contactor_status_t contactorStatus;
static can_message_t *contactorStatusHandle;
static msg_battery_pack_status_t packStatus;
static msg_battery_temperature_status_t temperatureStatus;
static msg_indicators_shutdown_status_t indicatorStatus;
static msg_battery_cell_limits_t cellLimits;
static msg_cell_voltages_t cellVoltages[CELL_VOLTAGE_PACKET_COUNT];
static msg_cell_temperatures_t cellTemperatures[CELL_TEMPERATURE_PACKET_COUNT];
static msg_allow_balance_command_t allowBalanceCommand;
static can_receive_message_t *allowBalanceHandle;
static msg_vcu_state_t vcuState;
static can_receive_message_t *vcuStateHandle;
static msg_hvc_charger_command_t chargerCommand;
static msg_charger_status_t chargerStatus;
static can_message_t *chargerCommandHandle;
static can_receive_message_t *chargerStatusHandle;
static bool chargerCommandRegistered;

static can_interface_t criticalCanBus = {
    .handle = &hfdcan1,
};

/*
 * The bare-metal CAN API allocates one small handle per scheduled packet.
 * Keep those handles in static RAM rather than relying on this firmware's
 * intentionally tiny newlib heap.
 */
static union {
    uint64_t alignment;
    uint8_t bytes[CAN_MESSAGE_POOL_SIZE];
} canMessagePool;
static size_t canMessagePoolUsed;

static void *canMessageAlloc(size_t size) {
    const size_t alignment = sizeof(uint64_t);
    size = (size + alignment - 1U) & ~(alignment - 1U);
    if (size > CAN_MESSAGE_POOL_SIZE - canMessagePoolUsed) return NULL;

    void *allocation = &canMessagePool.bytes[canMessagePoolUsed];
    canMessagePoolUsed += size;
    return allocation;
}

static void canMessageFree(void *allocation) {
    (void)allocation;
}

static void registerSendPacket(void *message, uint32_t id, uint16_t periodMs,
                               uint8_t dlc, CAN_pack_message_fn pack) {
    can_message_t *handle =
        can_get_message_handle(message, id, periodMs, dlc, pack);
    if (handle != NULL) can_register_send_packet(&criticalCanBus, handle);
}

static float nonnegativeReading(float reading) {
    return reading >= 0.0f ? reading : 0.0f;
}

static uint8_t unsignedTemperature(float temperature) {
    if (!(temperature >= 0.0f)) return 0U;
    if (temperature > 255.0f) return 255U;
    return (uint8_t)temperature;
}

void hvc_can_init(void) {
    canMessagePoolUsed = 0U;
    chargerCommandRegistered = false;
    can_reset_internals();

    can_config_t canConfig = {
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
        .tick_fn = (Tick_fn)HAL_GetTick,
        .add_filter_fn = (CAN_AddFilter_fn)HAL_FDCAN_ConfigFilter,
        .malloc_fn = canMessageAlloc,
        .free_fn = canMessageFree,
        .init_bit = FDCAN_CCCR_INIT,
        .device_id = DEVICE_ID_HVC,
        .write_memory_fn = NULL,
        .fw_update_begin_fn = NULL,
        .abort_update_fn = NULL,
    };

    criticalCanBus = (can_interface_t){
        .handle = &hfdcan1,
        .cccr_reg = &hfdcan1.Instance->CCCR,
    };

    can_init(&canConfig);
    can_register_interface(&criticalCanBus);

    contactorStatusHandle = can_get_message_handle(
        &contactorStatus, CONTACTOR_STATUS_ID,
        HVC_CONTACTOR_STATUS_PERIOD_MS,
        CONTACTOR_STATUS_DLC,
        (CAN_pack_message_fn)pack_contactor_status);
    if (contactorStatusHandle != NULL) {
        can_register_send_packet(&criticalCanBus, contactorStatusHandle);
    }
    registerSendPacket(&packStatus, BATTERY_PACK_STATUS_ID,
                       BATTERY_PACK_STATUS_FREQ, BATTERY_PACK_STATUS_DLC,
                       (CAN_pack_message_fn)pack_battery_pack_status);
    registerSendPacket(&temperatureStatus, BATTERY_TEMPERATURE_STATUS_ID,
                       BATTERY_TEMPERATURE_STATUS_FREQ,
                       BATTERY_TEMPERATURE_STATUS_DLC,
                       (CAN_pack_message_fn)pack_battery_temperature_status);
    registerSendPacket(&indicatorStatus, INDICATORS_SHUTDOWN_STATUS_ID,
                       INDICATORS_SHUTDOWN_STATUS_FREQ,
                       INDICATORS_SHUTDOWN_STATUS_DLC,
                       (CAN_pack_message_fn)pack_indicators_shutdown_status);
    registerSendPacket(&cellLimits, BATTERY_CELL_LIMITS_ID,
                       BATTERY_CELL_LIMITS_FREQ, BATTERY_CELL_LIMITS_DLC,
                       (CAN_pack_message_fn)pack_battery_cell_limits);

    for (uint32_t i = 0U; i < CELL_VOLTAGE_PACKET_COUNT; i++) {
        registerSendPacket(&cellVoltages[i], CELL_VOLTAGES_ID + i,
                           CELL_VOLTAGES_FREQ, CELL_VOLTAGES_DLC,
                           (CAN_pack_message_fn)pack_cell_voltages);
    }
    for (uint32_t i = 0U; i < CELL_TEMPERATURE_PACKET_COUNT; i++) {
        registerSendPacket(&cellTemperatures[i], CELL_TEMPERATURES_ID + i,
                           CELL_TEMPERATURES_FREQ, CELL_TEMPERATURES_DLC,
                           (CAN_pack_message_fn)pack_cell_temperatures);
    }

    allowBalanceHandle = can_get_receive_message_handle(
        &allowBalanceCommand, ALLOW_BALANCE_COMMAND_ID,
        (CAN_unpack_message_fn)unpack_allow_balance_command);
    if (allowBalanceHandle != NULL) {
        can_register_receive_packet(&criticalCanBus, allowBalanceHandle);
    }

    vcuStateHandle = can_get_receive_message_handle(
        &vcuState, VCU_STATE_ID,
        (CAN_unpack_message_fn)unpack_vcu_state);
    if (vcuStateHandle != NULL) {
        can_register_receive_packet(&criticalCanBus, vcuStateHandle);
    }

    chargerCommandHandle = can_get_message_handle(
        &chargerCommand, HVC_CHARGER_COMMAND_ID, HVC_CHARGER_COMMAND_FREQ,
        HVC_CHARGER_COMMAND_DLC,
        (CAN_pack_message_fn)pack_hvc_charger_command);
    chargerStatusHandle = can_get_receive_message_handle(
        &chargerStatus, CHARGER_STATUS_ID,
        (CAN_unpack_message_fn)unpack_charger_status);
    if (chargerStatusHandle != NULL) {
        can_register_receive_packet(&criticalCanBus, chargerStatusHandle);
    }

    HAL_FDCAN_ConfigGlobalFilter(&hfdcan1, FDCAN_REJECT, FDCAN_REJECT,
                                 FDCAN_REJECT_REMOTE,
                                 FDCAN_REJECT_REMOTE);

    can_start_interface(&criticalCanBus);
}

void hvc_can_service_tx(void) {
    can_service(&criticalCanBus);
}

void hvc_can_periodic(bool amsError, bool imdError, int state,
                      float deltaTime) {
    contactorStatus.hvc_state_machine = HVC_CAN_FORCE_ENERGIZED
        ? (uint8_t)HVC_STATE_ENERGIZED
        : (uint8_t)state;
    contactorStatus.positive_hv_contactor = isPosContactorClosed();
    contactorStatus.negative_hv_contactor = isNegContactorClosed();
    contactorStatus.precharge_contactor =
        state == HVC_STATE_PRECHARGING ||
        state == HVC_STATE_CHARGING_PRECHARGING;

    packStatus.pack_voltage = nonnegativeReading(getPackVoltageFromCells());
    packStatus.tractive_current = nonnegativeReading(getTractiveCurrent());
    packStatus.state_of_charge = nonnegativeReading(getSoc(deltaTime));
    packStatus.cell_top_temp = unsignedTemperature(getMaxTemp());
    packStatus.cell_bottom_temp = unsignedTemperature(getMinTemp());

    temperatureStatus.bus_bar_1_temp = nonnegativeReading(getBusBar1Temp());
    temperatureStatus.bus_bar_2_temp = nonnegativeReading(getBusBar2Temp());
    temperatureStatus.bus_bar_3_temp = nonnegativeReading(getBusBar3Temp());
    temperatureStatus.precharge_resistor_temp =
        nonnegativeReading(getPrechargeTemp());

    indicatorStatus.bms_error = amsError;
    indicatorStatus.imd_error = imdError;
    indicatorStatus.shutdown_leg_1 = isShutdownOneOk();
    indicatorStatus.shutdown_leg_2 = isShutdownTwoOk();
    indicatorStatus.shutdown_leg_3 = isShutdownThreeOk();
    indicatorStatus.shutdown_leg_4 = isShutdownFourOk();

    cellLimits.min_cell_voltage = nonnegativeReading(getMinCellVoltage());
    cellLimits.max_cell_voltage = nonnegativeReading(getMaxCellVoltage());

    carParked = allowBalanceHandle != NULL &&
                !message_timed_out(allowBalanceHandle,
                                   ALLOW_BALANCE_COMMAND_TIMEOUT_MS) &&
                allowBalanceCommand.allow_balance != 0U;

    for (uint32_t packet = 0U; packet < CELL_VOLTAGE_PACKET_COUNT; packet++) {
        const uint32_t cell = packet * 4U;
        cellVoltages[packet].voltage_i =
            nonnegativeReading(getCellVoltage(cell));
        cellVoltages[packet].voltage_i_1 =
            nonnegativeReading(getCellVoltage(cell + 1U));
        cellVoltages[packet].voltage_i_2 =
            nonnegativeReading(getCellVoltage(cell + 2U));
        cellVoltages[packet].voltage_i_3 =
            nonnegativeReading(getCellVoltage(cell + 3U));
    }

    for (uint32_t packet = 0U; packet < CELL_TEMPERATURE_PACKET_COUNT;
         packet++) {
        const uint32_t temperature = packet * 4U;
        cellTemperatures[packet].temp_i =
            nonnegativeReading(getCellTemperature(temperature));
        cellTemperatures[packet].temp_i_1 =
            nonnegativeReading(getCellTemperature(temperature + 1U));
        cellTemperatures[packet].temp_i_2 = temperature + 2U < 90U
            ? nonnegativeReading(getCellTemperature(temperature + 2U))
            : 0.0f;
        cellTemperatures[packet].temp_i_3 = temperature + 3U < 90U
            ? nonnegativeReading(getCellTemperature(temperature + 3U))
            : 0.0f;
    }

    hvc_can_service_tx();
}

static bool receiveStatus(can_receive_message_t *handle, uint32_t timeoutMs,
                          uint32_t *ageMs) {
    if (handle == NULL) {
        *ageMs = UINT32_MAX;
        return false;
    }

    *ageMs = HAL_GetTick() - handle->_latest_rx_ms;
    return !message_timed_out(handle, timeoutMs);
}

void hvc_can_get_rx_status(hvc_can_rx_status_t *status) {
    if (status == NULL) return;

    status->vcuStateValid = receiveStatus(
        vcuStateHandle, VCU_STATE_TIMEOUT_MS, &status->vcuStateAgeMs);
    status->prndlState = vcuState.prndl_state;
    status->stompFault = vcuState.stomp_fault != 0U;
    status->readyToDriveBuzzer = vcuState.ready_to_drive_buzzer != 0U;
    status->stateOfChargeEstimate = vcuState.state_of_charge_estimate;
    status->lineLockEnabled = vcuState.line_lock_enabled != 0U;
    status->eventMode = vcuState.event_mode;
}

void hvc_can_get_tx_status(hvc_can_tx_status_t *status) {
    if (status == NULL) return;

    status->interfaceStarted = criticalCanBus._started;
    status->contactorStatusRegistered = contactorStatusHandle != NULL;
    status->contactorStateForced = HVC_CAN_FORCE_ENERGIZED != 0;
    status->contactorStatusAgeMs = contactorStatusHandle != NULL
        ? HAL_GetTick() - contactorStatusHandle->_last_tx_time_ms
        : UINT32_MAX;
    status->messagesQueued = criticalCanBus._messages_sent;
    status->droppedPackets = criticalCanBus.dropped_packets;
    pack_contactor_status(&contactorStatus, status->contactorStatusData);
}

bool hvc_can_is_charger_connected(void) {
    if (chargerStatusHandle == NULL) return false;

    const bool connected =
        !message_timed_out(chargerStatusHandle, CHARGER_STATUS_TIMEOUT_MS);
    if (connected && !chargerCommandRegistered &&
        chargerCommandHandle != NULL) {
        can_register_send_packet(&criticalCanBus, chargerCommandHandle);
        chargerCommandRegistered = true;
    }
    return connected;
}

void hvc_can_set_charger_command(float maxChargeVoltage,
                                 float maxChargeCurrent, bool imdLed,
                                 bool bmsLed, bool enable) {
    chargerCommand.max_charge_voltage = maxChargeVoltage;
    chargerCommand.max_charge_current = maxChargeCurrent;
    chargerCommand.imd_led_state = imdLed;
    chargerCommand.bms_led_state = bmsLed;
    chargerCommand.charger_enable = enable;
}
