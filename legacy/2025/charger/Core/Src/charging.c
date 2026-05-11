//
// Created by Ashwin Kuppahally on 3/24/25.
// Ported 2026: speaks the new HVC protocol on CAN2 (std IDs 0x200 RX /
// 0x201 TX, little-endian, see drivers/longhorn-lib/config/can_packets.csv)
// and drives an ELCON-style charger on CAN1 (ext IDs 0x1806E5F4 TX /
// 0x18FF50E5 RX, big-endian, 0.1 V / 0.1 A).
//
// Architecture (unchanged): CAN1 (hcan1) <-> ELCON, CAN2 (hcan2) <-> HVC.
//
// vs. the original relay implementation:
//   - HVC RX moved from std 0x2FE to std 0x200 (target_v u16 LE 0.01V /
//     current_limit u16 LE 0.01A / flags / seq).
//   - HVC TX moved from std 0x114 to std 0x201 (charger_v / charger_c /
//     flags / state / plug / seq_echo).
//   - The charger now owns its own state machine and the plug status it
//     reports; HVC commands are decoded and re-encoded into ELCON's units
//     rather than relayed byte-for-byte.
//

#include "charging.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "can.h"     // hcan1, hcan2 (also pulls in main.h: Error_Handler, HAL)
#include "main.h"
#include "pilot.h"

// ---------------------------------------------------------------------------
// Protocol constants
// ---------------------------------------------------------------------------

#define HVC_CHARGE_COMMAND_ID    0x200u   // HVC -> charger (std)
#define HVC_CHARGER_STATUS_ID    0x201u   // charger -> HVC (std)
#define HVC_INDICATORS_STATUS_ID 0x134u   // HVC -> * "Indicators + Shutdown Status" (std)

#define ELCON_COMMAND_ID       0x1806E5F4u  // charger -> ELCON (ext)
#define ELCON_STATUS_ID        0x18FF50E5u  // ELCON   -> charger (ext)

#define HVC_COMMAND_DLC        6u
#define HVC_STATUS_DLC         8u
#define ELCON_DLC              8u

// HVC -> charger flags (CSV row 0x200).
#define CHG_FLAG_ENABLE        (1u << 0)
#define CHG_FLAG_HARD_STOP     (1u << 1)

// charger -> HVC flags (CSV row 0x201, mirroring the ELCON status flag byte).
// Per the ELCON protocol (datasheet p.10) bit 3 is "battery not connected or
// connected with reversed polarity" — a genuine wiring fault, not a transient.
// Bits 5..7 are unspecified by the datasheet, so there is no polarity bit 5.
#define CHARGER_FLAG_HW_FAIL            (1u << 0)
#define CHARGER_FLAG_OVERTEMP           (1u << 1)
#define CHARGER_FLAG_INPUT_V_FAULT      (1u << 2)
#define CHARGER_FLAG_BATTERY_DISCONNECT (1u << 3)
#define CHARGER_FLAG_COMM_FAULT         (1u << 4)

// charger_state enum (CSV row 0x201).
typedef enum {
    CHARGER_STATE_IDLE       = 0,
    CHARGER_STATE_CONNECTING = 1,
    CHARGER_STATE_CC         = 2,
    CHARGER_STATE_CV         = 3,
    CHARGER_STATE_COMPLETE   = 4,
    CHARGER_STATE_FAULT      = 5,
} charger_state_t;

// plug_status enum (CSV row 0x201).
typedef enum {
    PLUG_STATUS_UNPLUGGED = 0,
    PLUG_STATUS_PLUGGED   = 1,
    PLUG_STATUS_READY     = 2,
    PLUG_STATUS_FAULT     = 3,
} plug_status_t;

// ---------------------------------------------------------------------------
// Behavioral tunables
// ---------------------------------------------------------------------------

// At/below this commanded current we report CV (HVC's CC/CV taper drops
// current toward its termination value near the CV target). Above, CC.
#define CV_THRESHOLD_A          1.0f

// HVC dropping enable while we were in CV with negligible current = done.
#define COMPLETE_THRESHOLD_A    0.1f

// Maximum tolerated gap between Charge Command frames before we cut current
// and assert COMM_FAULT. CHARGE_COMMAND_FREQ is 10 Hz (100 ms).
#define HVC_COMM_TIMEOUT_MS     500u

// The ELCON broadcasts its status frame at roughly 1 Hz, so the freshness
// window for it must be several status periods — using HVC_COMM_TIMEOUT_MS
// (500 ms) here would make plug status flicker UNPLUGGED between frames.
#define ELCON_STATUS_TIMEOUT_MS 3000u

#define TX_TO_HVC_PERIOD_S      0.10f   // 10 Hz, matches 0x201 freq in CSV
#define TX_TO_ELCON_PERIOD_S    0.05f   // 20 Hz, ELCON canonical

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

static CAN_HandleTypeDef *elcon = &hcan1;
static CAN_HandleTypeDef *hvc   = &hcan2;

// Most recent Charge Command from HVC.
static float    hvc_target_v_v = 0.0f;
static float    hvc_current_a  = 0.0f;
static uint8_t  hvc_flags      = 0;
static uint8_t  hvc_seq        = 0;
static uint32_t hvc_last_rx_ms = 0;

// Most recent Indicators + Shutdown Status from HVC (0x134). Drives the
// front-panel BMS/IMD LEDs via getAmsError() / getImdError().
static bool     hvc_ams_error        = false;  // "BMS Error" bit, byte 0
static bool     hvc_imd_error        = false;  // "IMD Error" bit, byte 1
static uint32_t hvc_indicators_rx_ms = 0;

// Most recent ELCON status.
static float    elcon_v          = 0.0f;
static float    elcon_c          = 0.0f;
static uint8_t  elcon_flags      = 0;
static uint32_t elcon_last_rx_ms = 0;

// What we last reported to HVC (used for a sticky COMPLETE label).
static charger_state_t reported_state = CHARGER_STATE_IDLE;

static float tx_to_hvc_timer   = 0.0f;
static float tx_to_elcon_timer = 0.0f;

// ---------------------------------------------------------------------------
// Byte helpers
// ---------------------------------------------------------------------------

static inline uint16_t rd_u16_le(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static inline void wr_u16_le(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}
static inline uint16_t rd_u16_be(const uint8_t *p) {
    return ((uint16_t)p[0] << 8) | (uint16_t)p[1];
}
static inline void wr_u16_be(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)((v >> 8) & 0xFFu);
    p[1] = (uint8_t)(v & 0xFFu);
}

static inline uint16_t f_to_u16_clamped(float x) {
    if (x < 0.0f)      return 0u;
    if (x > 65535.0f)  return 65535u;
    return (uint16_t)(x + 0.5f);
}

static inline uint32_t now_ms(void) { return HAL_GetTick(); }

// ---------------------------------------------------------------------------
// CAN filter setup
// ---------------------------------------------------------------------------

// Accept exactly one std ID into the given FIFO.
static void can_filter_std(CAN_HandleTypeDef *h, uint32_t bank, uint16_t id,
                           uint32_t fifo) {
    CAN_FilterTypeDef f = {0};
    f.FilterBank           = bank;
    f.SlaveStartFilterBank = 14u;  // CAN1 owns banks 0..13, CAN2 owns 14..27
    f.FilterMode           = CAN_FILTERMODE_IDMASK;
    f.FilterScale          = CAN_FILTERSCALE_32BIT;
    // bxCAN 32-bit filter layout: STDID[10:0] sits in bits 31:21.
    f.FilterIdHigh         = (uint16_t)((id & 0x7FFu) << 5);
    f.FilterIdLow          = 0;
    f.FilterMaskIdHigh     = (uint16_t)(0x7FFu << 5);
    f.FilterMaskIdLow      = (uint16_t)CAN_ID_EXT;  // require IDE=0 (std)
    f.FilterFIFOAssignment = fifo;
    f.FilterActivation     = ENABLE;
    if (HAL_CAN_ConfigFilter(h, &f) != HAL_OK) Error_Handler();
}

// Accept exactly one ext ID into the given FIFO.
static void can_filter_ext(CAN_HandleTypeDef *h, uint32_t bank, uint32_t id,
                           uint32_t fifo) {
    CAN_FilterTypeDef f = {0};
    f.FilterBank           = bank;
    f.SlaveStartFilterBank = 14u;  // CAN1 owns banks 0..13, CAN2 owns 14..27
    f.FilterMode           = CAN_FILTERMODE_IDMASK;
    f.FilterScale          = CAN_FILTERSCALE_32BIT;
    f.FilterIdHigh         = (uint16_t)((id >> 13) & 0xFFFFu);
    f.FilterIdLow          = (uint16_t)(((id << 3) & 0xFFF8u) | CAN_ID_EXT);
    f.FilterMaskIdHigh     = (uint16_t)((0x1FFFFFFFu >> 13) & 0xFFFFu);
    f.FilterMaskIdLow      = (uint16_t)(((0x1FFFFFFFu << 3) & 0xFFF8u) | CAN_ID_EXT);
    f.FilterFIFOAssignment = fifo;
    f.FilterActivation     = ENABLE;
    if (HAL_CAN_ConfigFilter(h, &f) != HAL_OK) Error_Handler();
}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

void charging_init(void) {
    // ELCON bus: CAN1 filter bank 0 -> FIFO0.
    can_filter_ext(elcon, 0u, ELCON_STATUS_ID, CAN_RX_FIFO0);
    // HVC bus: CAN2 filter banks 14/15 -> FIFO1 (Charge Command + Indicators).
    can_filter_std(hvc, 14u, HVC_CHARGE_COMMAND_ID,    CAN_RX_FIFO1);
    can_filter_std(hvc, 15u, HVC_INDICATORS_STATUS_ID, CAN_RX_FIFO1);

    if (HAL_CAN_Start(elcon) != HAL_OK) Error_Handler();
    if (HAL_CAN_Start(hvc)   != HAL_OK) Error_Handler();
}

// ---------------------------------------------------------------------------
// RX
// ---------------------------------------------------------------------------

static void drain_hvc_rx(void) {
    while (HAL_CAN_GetRxFifoFillLevel(hvc, CAN_RX_FIFO1) > 0u) {
        CAN_RxHeaderTypeDef hdr = {0};
        uint8_t data[8] = {0};
        if (HAL_CAN_GetRxMessage(hvc, CAN_RX_FIFO1, &hdr, data) != HAL_OK) {
            continue;
        }
        if (hdr.IDE != CAN_ID_STD) continue;

        if (hdr.StdId == HVC_CHARGE_COMMAND_ID) {
            hvc_target_v_v = (float)rd_u16_le(&data[0]) * 0.01f;
            hvc_current_a  = (float)rd_u16_le(&data[2]) * 0.01f;
            hvc_flags      = data[4];
            hvc_seq        = data[5];
            hvc_last_rx_ms = now_ms();
        } else if (hdr.StdId == HVC_INDICATORS_STATUS_ID) {
            hvc_ams_error        = (data[0] != 0u);  // byte 0 = BMS Error
            hvc_imd_error        = (data[1] != 0u);  // byte 1 = IMD Error
            hvc_indicators_rx_ms = now_ms();
        }
    }
}

static void drain_elcon_rx(void) {
    while (HAL_CAN_GetRxFifoFillLevel(elcon, CAN_RX_FIFO0) > 0u) {
        CAN_RxHeaderTypeDef hdr = {0};
        uint8_t data[8] = {0};
        if (HAL_CAN_GetRxMessage(elcon, CAN_RX_FIFO0, &hdr, data) != HAL_OK) {
            continue;
        }
        if (hdr.IDE != CAN_ID_EXT || hdr.ExtId != ELCON_STATUS_ID) continue;

        // Current word's MSB is a sign bit (datasheet p.9 BYTE3 footnote:
        // 0 = charging, 1 = discharging). Mask it before scaling so a stray
        // discharge flag doesn't read as ~3276 A.
        uint16_t raw_c   = rd_u16_be(&data[2]);
        bool discharging = (raw_c & 0x8000u) != 0u;
        elcon_v          = (float)rd_u16_be(&data[0]) * 0.1f;
        elcon_c          = (float)(raw_c & 0x7FFFu) * 0.1f * (discharging ? -1.0f : 1.0f);
        elcon_flags      = data[4];  // bit layout matches CHARGER_FLAG_*
        elcon_last_rx_ms = now_ms();
    }
}

// ---------------------------------------------------------------------------
// Status synthesis
// ---------------------------------------------------------------------------

// No J1772 proximity-pin ADC on this board, so plug presence is inferred:
// UNPLUGGED until we've heard from ELCON, PLUGGED while ELCON is alive but
// HVC isn't talking, READY once both sides are talking.
// TODO: replace with a real proximity read once that hardware lands.
static plug_status_t compute_plug_status(uint32_t now) {
    bool elcon_fresh = elcon_last_rx_ms != 0u &&
                       (now - elcon_last_rx_ms) < ELCON_STATUS_TIMEOUT_MS;
    bool hvc_fresh   = hvc_last_rx_ms   != 0u &&
                       (now - hvc_last_rx_ms)   < HVC_COMM_TIMEOUT_MS;

    if (!elcon_fresh) return PLUG_STATUS_UNPLUGGED;
    if (!hvc_fresh)   return PLUG_STATUS_PLUGGED;
    return PLUG_STATUS_READY;
}

static charger_state_t compute_state(uint8_t status_flags, plug_status_t plug) {
    // Every defined ELCON status bit is a genuine fault, including bit 3
    // (battery missing / reverse polarity).
    const uint8_t fault_mask = CHARGER_FLAG_HW_FAIL |
                               CHARGER_FLAG_OVERTEMP |
                               CHARGER_FLAG_INPUT_V_FAULT |
                               CHARGER_FLAG_BATTERY_DISCONNECT |
                               CHARGER_FLAG_COMM_FAULT;
    if (status_flags & fault_mask) return CHARGER_STATE_FAULT;

    // Plugged but HVC isn't commanding yet -> still bringing the link up.
    if (plug == PLUG_STATUS_PLUGGED) return CHARGER_STATE_CONNECTING;

    bool enabled = (hvc_flags & CHG_FLAG_ENABLE) != 0u;
    if (!enabled) {
        if (reported_state == CHARGER_STATE_CV &&
            hvc_current_a < COMPLETE_THRESHOLD_A) {
            return CHARGER_STATE_COMPLETE;
        }
        return CHARGER_STATE_IDLE;
    }

    return (hvc_current_a <= CV_THRESHOLD_A) ? CHARGER_STATE_CV
                                             : CHARGER_STATE_CC;
}

// ---------------------------------------------------------------------------
// TX
// ---------------------------------------------------------------------------

static void send_to_elcon(uint32_t now) {
    bool active = (hvc_flags & CHG_FLAG_ENABLE) != 0u &&
                  (hvc_flags & CHG_FLAG_HARD_STOP) == 0u &&
                  hvc_last_rx_ms != 0u &&
                  (now - hvc_last_rx_ms) < HVC_COMM_TIMEOUT_MS;

    uint8_t data[ELCON_DLC] = {0};
    uint16_t raw_v = f_to_u16_clamped(hvc_target_v_v * 10.0f);
    uint16_t raw_c = active ? f_to_u16_clamped(hvc_current_a * 10.0f) : 0u;

    wr_u16_be(&data[0], raw_v);
    wr_u16_be(&data[2], raw_c);
    data[4] = active ? 0x00u : 0x01u;  // 0=run, 1=battery-protect-stop
    // data[5..7] reserved = 0

    CAN_TxHeaderTypeDef hdr = {0};
    hdr.ExtId = ELCON_COMMAND_ID;
    hdr.IDE   = CAN_ID_EXT;
    hdr.RTR   = CAN_RTR_DATA;
    hdr.DLC   = ELCON_DLC;
    uint32_t mb = 0;
    (void)HAL_CAN_AddTxMessage(elcon, &hdr, data, &mb);  // drop on full mailbox
}

static void send_to_hvc(uint8_t status_flags, charger_state_t state,
                        plug_status_t plug) {
    uint8_t data[HVC_STATUS_DLC] = {0};
    wr_u16_le(&data[0], f_to_u16_clamped(elcon_v * 100.0f));
    wr_u16_le(&data[2], f_to_u16_clamped(elcon_c * 100.0f));
    data[4] = status_flags;
    data[5] = (uint8_t)state;
    data[6] = (uint8_t)plug;
    data[7] = hvc_seq;  // echo back the seq we last received

    CAN_TxHeaderTypeDef hdr = {0};
    hdr.StdId = HVC_CHARGER_STATUS_ID;
    hdr.IDE   = CAN_ID_STD;
    hdr.RTR   = CAN_RTR_DATA;
    hdr.DLC   = HVC_STATUS_DLC;
    uint32_t mb = 0;
    (void)HAL_CAN_AddTxMessage(hvc, &hdr, data, &mb);
}

// ---------------------------------------------------------------------------
// Periodic
// ---------------------------------------------------------------------------

void charging_periodic(float dt) {
    drain_hvc_rx();
    drain_elcon_rx();

    uint32_t now = now_ms();

    // ELCON's flag byte already maps to CHARGER_FLAG_*; layer COMM_FAULT on
    // top if HVC has gone quiet.
    uint8_t status_flags = elcon_flags;
    bool hvc_stale = hvc_last_rx_ms == 0u ||
                     (now - hvc_last_rx_ms) >= HVC_COMM_TIMEOUT_MS;
    if (hvc_stale) status_flags |= CHARGER_FLAG_COMM_FAULT;

    plug_status_t plug = compute_plug_status(now);
    reported_state     = compute_state(status_flags, plug);

    tx_to_elcon_timer += dt;
    if (tx_to_elcon_timer >= TX_TO_ELCON_PERIOD_S) {
        tx_to_elcon_timer = 0.0f;
        send_to_elcon(now);
    }

    tx_to_hvc_timer += dt;
    if (tx_to_hvc_timer >= TX_TO_HVC_PERIOD_S) {
        tx_to_hvc_timer = 0.0f;
        send_to_hvc(status_flags, reported_state, plug);
    }
}

// ---------------------------------------------------------------------------
// Front-panel LED hooks. Reflect the BMS/IMD error bits HVC broadcasts in its
// "Indicators + Shutdown Status" frame (0x134). If that frame goes stale we
// report "no error" — a dead HVC link surfaces elsewhere as a charge cut /
// COMM_FAULT, not on these LEDs.
// ---------------------------------------------------------------------------

static bool hvc_indicators_fresh(void) {
    return hvc_indicators_rx_ms != 0u &&
           (now_ms() - hvc_indicators_rx_ms) < HVC_COMM_TIMEOUT_MS;
}

bool getAmsError(void) { return hvc_indicators_fresh() && hvc_ams_error; }
bool getImdError(void) { return hvc_indicators_fresh() && hvc_imd_error; }
