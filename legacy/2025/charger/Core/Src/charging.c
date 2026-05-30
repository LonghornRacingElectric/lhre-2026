//
// Created by Ashwin Kuppahally on 3/24/25.
// Updated 2026: HVC CAN integration via 0x050 / 0x051
//

#include "charging.h"

#include <stdbool.h>
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

// ---- State from HVC command (0x050) ----------------------------------------
static float    cmd_voltage_V  = 0.0f;
static float    cmd_current_A  = 0.0f;
static bool     cmd_enable     = false;
static uint8_t  cmd_imd_led    = 0;
static uint8_t  cmd_bms_led    = 0;

// ---- State from Elcon status (0x18FF50E5) ----------------------------------
static float    elcon_voltage_V = 0.0f;
static float    elcon_current_A = 0.0f;
static uint8_t  elcon_status    = 0xFF; // 0x00 = charging normally

// ---- Error flags (read by main.c for LED blinking) -------------------------
static uint8_t amsError = 0;
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
        if (hdr.StdId != LHR_HVC_CMD_ID) continue;

        // 0x050 packet (little-endian, 0.01 V / 0.01 A — matches pack_hvc_charger_command):
        // [0-1] max_charge_voltage   uint16  0.01 V
        // [2-3] max_charge_current   uint16  0.01 A
        // [4]   imd_led              uint8   bool
        // [5]   bms_led              uint8   bool
        // [6]   charger_enable       uint8   bool
        uint16_t raw_v = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
        uint16_t raw_a = (uint16_t)data[2] | ((uint16_t)data[3] << 8);
        cmd_voltage_V = raw_v * 0.01f;
        cmd_current_A = raw_a * 0.01f;
        cmd_imd_led   = data[4];
        cmd_bms_led   = data[5];
        cmd_enable    = data[6] != 0;

        // Mirror HVC-commanded LED states to the local error flags so that
        // main.c keeps the IMD / BMS indicator LEDs in sync with the HVC.
        imdError = cmd_imd_led;
        amsError = cmd_bms_led;
    }
}


// Drain FIFO0 on the Elcon bus, storing the latest charger status.
static void recv_elcon_status(void)
{
    CAN_RxHeaderTypeDef hdr;
    uint8_t data[8];

    while (HAL_CAN_GetRxFifoFillLevel(elcon, CAN_RX_FIFO0) > 0) {
        if (HAL_CAN_GetRxMessage(elcon, CAN_RX_FIFO0, &hdr, data) != HAL_OK) continue;
        if (hdr.ExtId != ELCON_STATUS_ID) continue;

        // Elcon status (big-endian, 0.1 V / 0.1 A):
        // [0-1] output voltage, [2-3] output current, [4] status flags
        // Status byte: 0x00 = charging normally; any set bit = fault/not-charging
        uint16_t raw_v = ((uint16_t)data[0] << 8) | data[1];
        uint16_t raw_a = ((uint16_t)data[2] << 8) | data[3];
        elcon_voltage_V = raw_v * 0.1f;
        elcon_current_A = raw_a * 0.1f;
        elcon_status    = data[4];
    }
}


// Translate the HVC command into the Elcon wire format and transmit both the
// command frame and the legacy 0x114 heartbeat the Elcon expects.
static void send_elcon_command(void)
{
    CAN_TxHeaderTypeDef hdr;
    uint32_t mailbox;

    // Elcon command (0x1806E5F4): big-endian, 0.1 V / 0.1 A
    // HVC sends 0.01 V/A, Elcon wants 0.1 V/A → divide raw by 10
    uint16_t raw_v = (uint16_t)(cmd_voltage_V / 0.1f);
    uint16_t raw_a = (uint16_t)(cmd_current_A / 0.1f);

    uint8_t cmd[8] = {
        (raw_v >> 8) & 0xFF,
         raw_v       & 0xFF,
        (raw_a >> 8) & 0xFF,
         raw_a       & 0xFF,
        cmd_enable ? 0x00 : 0x01,  // 0x00 = charge, 0x01 = stop
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
    // Elcon sends 0.1 V/A → multiply by 10 to get 0.01 V/A raw values
    uint16_t raw_v = (uint16_t)(elcon_voltage_V * 10.0f);
    uint16_t raw_a = (uint16_t)(elcon_current_A * 10.0f);
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


bool getAmsError(void) { return amsError != 0; }
bool getImdError(void) { return imdError != 0; }
