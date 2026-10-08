//
// Created by Ashwin Kuppahally on 3/24/25.
// Updated 2026: HVC CAN integration via 0x050 / 0x051
//

#include "charging.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "can.h"
#include "led.h"
#include "pilot.h"

// CAN1 = Elcon charger hardware (500 kbps, proprietary extended-ID protocol)
static CAN_HandleTypeDef *elcon = &hcan1;

// CAN2 = LHR vehicle bus (standard-ID, 0x050 / 0x051)
static CAN_HandleTypeDef *hvc_bus = &hcan2;

extern int rainbow;

// ---- Elcon protocol IDs (extended, big-endian, 0.1 V / 0.1 A) -------------
#define ELCON_CMD_ID      0x1806E5F4UL  // charger ← us
#define ELCON_STATUS_ID   0x18FF50E5UL  // charger → us
#define ELCON_PLUG_ID     0x114         // heartbeat the Elcon expects (standard)
#define ELCON_PERIOD_S    0.05f         // 20 Hz minimum or charger times out

// ---- LHR bus IDs (standard, little-endian, 0.01 V / 0.01 A) ---------------
#define LHR_HVC_CMD_ID    0x050         // HVC → us
#define LHR_STATUS_ID     0x051         // us → HVC
#define LHR_STATUS_PERIOD_S 0.1f        // 10 Hz

// HVC commands are in 0.01 V / 0.01 A. Bound them independently of the HVC
// firmware so a bad or mismatched command cannot exceed the new pack limits.
#define MAX_CHARGE_VOLTAGE_CENTI_V 56000u
#define MAX_CHARGE_CURRENT_CENTI_A 2000u
#define HVC_COMMAND_TIMEOUT_MS 500u

// ---- State from HVC command (0x050) ----------------------------------------
static uint16_t cmd_voltage_cV = 0;
static uint16_t cmd_current_cA = 0;
static bool     cmd_enable     = false;
static bool     cmd_seen       = false;
static uint32_t cmd_last_rx_ms = 0;
static uint8_t  cmd_imd_led    = 0;
static uint8_t  cmd_bms_led    = 0;

// ---- State from Elcon status (0x18FF50E5) ----------------------------------
static uint16_t elcon_voltage_deci_V = 0;
static uint16_t elcon_current_deci_A = 0;
static uint8_t  elcon_status    = 0xFF; // 0x00 = charging normally

// ---- Error flags (read by main.c for LED blinking) -------------------------
static uint8_t bmsError = 0;
static uint8_t imdError = 0;


void charging_init(int a)
{
    (void)a;

    // HVC bus filter: standard ID 0x050, FIFO1
    // In 32-bit mask mode, STID sits in bits [31:21] of the filter register.
    // FilterIdHigh = FR1[31:16], so STID maps to FilterIdHigh[15:5].
    CAN_FilterTypeDef hvc_filter = {
        .FilterBank           = 14,
        .FilterMode           = CAN_FILTERMODE_IDMASK,
        .FilterScale          = CAN_FILTERSCALE_32BIT,
        .FilterIdHigh         = (LHR_HVC_CMD_ID << 5) & 0xFFFF,
        .FilterIdLow          = 0x0000,             // IDE = 0 (standard)
        .FilterMaskIdHigh     = (0x7FF    << 5) & 0xFFFF,
        .FilterMaskIdLow      = 0x0000,
        .FilterFIFOAssignment = CAN_RX_FIFO1,
        .FilterActivation     = ENABLE,
        .SlaveStartFilterBank = 14, // CAN1: banks 0-13; CAN2: banks 14-27
    };
    if (HAL_CAN_ConfigFilter(hvc_bus, &hvc_filter) != HAL_OK) {
        Error_Handler();
    }

    // Elcon filter: extended ID 0x18FF50E5, FIFO0
    CAN_FilterTypeDef elcon_filter = {
        .FilterBank           = 0,
        .FilterMode           = CAN_FILTERMODE_IDMASK,
        .FilterScale          = CAN_FILTERSCALE_32BIT,
        .FilterIdHigh         = (ELCON_STATUS_ID >> 13) & 0xFFFF,
        .FilterIdLow          = (ELCON_STATUS_ID <<  3) & 0xFFF8,
        .FilterMaskIdHigh     = (0x1FFFFFFF >> 13) & 0xFFFF,
        .FilterMaskIdLow      = (0x1FFFFFFF <<  3) & 0xFFF8,
        .FilterFIFOAssignment = CAN_RX_FIFO0,
        .FilterActivation     = ENABLE,
        .SlaveStartFilterBank = 14,
    };
    if (HAL_CAN_ConfigFilter(elcon, &elcon_filter) != HAL_OK) {
        Error_Handler();
    }

    HAL_CAN_Start(elcon);
    HAL_CAN_Start(hvc_bus);
}


// Drain FIFO1 on the HVC bus, updating stored command state from 0x050.
static void recv_hvc_command(void)
{
    CAN_RxHeaderTypeDef hdr;
    uint8_t data[8];

    while (HAL_CAN_GetRxFifoFillLevel(hvc_bus, CAN_RX_FIFO1) > 0) {
        if (HAL_CAN_GetRxMessage(hvc_bus, CAN_RX_FIFO1, &hdr, data) != HAL_OK) continue;
        if (hdr.IDE != CAN_ID_STD || hdr.RTR != CAN_RTR_DATA ||
            hdr.StdId != LHR_HVC_CMD_ID || hdr.DLC < 7) continue;

        // 0x050 packet (little-endian, 0.01 V / 0.01 A — matches pack_hvc_charger_command):
        // [0-1] max_charge_voltage   uint16  0.01 V
        // [2-3] max_charge_current   uint16  0.01 A
        // [4]   imd_led              uint8   bool
        // [5]   bms_led              uint8   bool
        // [6]   charger_enable       uint8   bool
        uint16_t raw_v = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
        uint16_t raw_a = (uint16_t)data[2] | ((uint16_t)data[3] << 8);
        cmd_voltage_cV = raw_v > MAX_CHARGE_VOLTAGE_CENTI_V
                             ? MAX_CHARGE_VOLTAGE_CENTI_V : raw_v;
        cmd_current_cA = raw_a > MAX_CHARGE_CURRENT_CENTI_A
                             ? MAX_CHARGE_CURRENT_CENTI_A : raw_a;
        cmd_imd_led   = data[4];
        cmd_bms_led   = data[5];
        cmd_enable    = data[6] != 0;
        cmd_last_rx_ms = HAL_GetTick();
        cmd_seen = true;

        // Mirror HVC-commanded LED states to the local error flags so that
        // main.c keeps the IMD / BMS indicator LEDs in sync with the HVC.
        imdError = cmd_imd_led;
        bmsError = cmd_bms_led;
    }
}

static bool charge_command_active(void)
{
    if (!cmd_seen || !cmd_enable) return false;
    if ((uint32_t)(HAL_GetTick() - cmd_last_rx_ms) > HVC_COMMAND_TIMEOUT_MS) {
        cmd_enable = false; // A fresh enable packet is required to restart.
        return false;
    }
    return cmd_voltage_cV >= 10u && cmd_current_cA >= 10u;
}


// Drain FIFO0 on the Elcon bus, storing the latest charger status.
static void recv_elcon_status(void)
{
    CAN_RxHeaderTypeDef hdr;
    uint8_t data[8];

    while (HAL_CAN_GetRxFifoFillLevel(elcon, CAN_RX_FIFO0) > 0) {
        if (HAL_CAN_GetRxMessage(elcon, CAN_RX_FIFO0, &hdr, data) != HAL_OK) continue;
        if (hdr.IDE != CAN_ID_EXT || hdr.RTR != CAN_RTR_DATA ||
            hdr.ExtId != ELCON_STATUS_ID || hdr.DLC < 5) continue;

        // Elcon status (big-endian, 0.1 V / 0.1 A):
        // [0-1] output voltage, [2-3] output current, [4] status flags
        // Status byte: 0x00 = charging normally; any set bit = fault/not-charging
        uint16_t raw_v = ((uint16_t)data[0] << 8) | data[1];
        uint16_t raw_a = ((uint16_t)data[2] << 8) | data[3];
        elcon_voltage_deci_V = raw_v;
        elcon_current_deci_A = raw_a;
        elcon_status    = data[4];
    }
}


// Translate the HVC command into the Elcon wire format and transmit both the
// command frame and the legacy 0x114 heartbeat the Elcon expects.
static void send_elcon_command(void)
{
    CAN_TxHeaderTypeDef hdr = {0};
    uint32_t mailbox;

    // Elcon command (0x1806E5F4): big-endian, 0.1 V / 0.1 A
    // HVC sends 0.01 V/A, Elcon wants 0.1 V/A → divide raw by 10.
    // A stale or disabled command sends zero limits and an explicit stop.
    bool enabled = charge_command_active();
    uint16_t raw_v = enabled ? cmd_voltage_cV / 10u : 0u;
    uint16_t raw_a = enabled ? cmd_current_cA / 10u : 0u;

    uint8_t cmd[8] = {
        (raw_v >> 8) & 0xFF,
         raw_v       & 0xFF,
        (raw_a >> 8) & 0xFF,
         raw_a       & 0xFF,
        enabled ? 0x00 : 0x01,  // 0x00 = charge, 0x01 = stop
        0x00, 0x00, 0x00,
    };

    hdr.ExtId = ELCON_CMD_ID;
    hdr.IDE   = CAN_ID_EXT;
    hdr.RTR   = CAN_RTR_DATA;
    hdr.DLC   = 8;
    HAL_CAN_AddTxMessage(elcon, &hdr, cmd, &mailbox);

    // 0x114 heartbeat — Elcon expects this on the same bus
    static uint8_t plug_pkt[8] = {1, 0, 0, 0, 0, 0, 0, 0};
    hdr.StdId = ELCON_PLUG_ID;
    hdr.IDE   = CAN_ID_STD;
    hdr.RTR   = CAN_RTR_DATA;
    hdr.DLC   = 8;
    HAL_CAN_AddTxMessage(elcon, &hdr, plug_pkt, &mailbox);
}


// Report the latest Elcon status back to HVC as 0x051.
static void send_hvc_status(void)
{
    // 0x051 packet (little-endian, 0.01 V / 0.01 A — matches pack_charger_status):
    // [0-1] charger_voltage   uint16  0.01 V
    // [2-3] charger_current   uint16  0.01 A
    // [4]   charger_enabled   uint8   bool
    // Elcon raw units are 0.1 V/A; HVC raw units are 0.01 V/A.
    // Multiply the raw values by 10, saturating the 16-bit HVC fields.
    uint16_t raw_v = elcon_voltage_deci_V > UINT16_MAX / 10u
                         ? UINT16_MAX : elcon_voltage_deci_V * 10u;
    uint16_t raw_a = elcon_current_deci_A > UINT16_MAX / 10u
                         ? UINT16_MAX : elcon_current_deci_A * 10u;
    uint8_t  enabled = (elcon_status == 0x00) ? 1 : 0;

    uint8_t payload[5] = {
         raw_v       & 0xFF,
        (raw_v >> 8) & 0xFF,
         raw_a       & 0xFF,
        (raw_a >> 8) & 0xFF,
        enabled,
    };

    CAN_TxHeaderTypeDef hdr = {
        .StdId = LHR_STATUS_ID,
        .IDE   = CAN_ID_STD,
        .RTR   = CAN_RTR_DATA,
        .DLC   = 5,
    };
    uint32_t mailbox;
    HAL_CAN_AddTxMessage(hvc_bus, &hdr, payload, &mailbox);
}


void charging_periodic(float dt)
{
    static float elcon_accum = 0.0f;
    static float hvc_accum   = 0.0f;

    recv_hvc_command();
    recv_elcon_status();

    elcon_accum += dt;
    hvc_accum   += dt;

    if (elcon_accum >= ELCON_PERIOD_S) {
        elcon_accum = 0.0f;
        send_elcon_command();
    }

    if (hvc_accum >= LHR_STATUS_PERIOD_S) {
        hvc_accum = 0.0f;
        send_hvc_status();
    }
}


bool getBmsError(void) { return bmsError != 0; }
bool getImdError(void) { return imdError != 0; }
bool getEnabled() { return charge_command_active(); }
