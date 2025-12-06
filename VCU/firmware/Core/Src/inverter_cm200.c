#include "inverter_cm200.h"
#include "longhorn/rtos/logger.h"
#include <math.h>
#include <string.h>

// CAN IDs for CM200DZ based on 2025 data flow
#define CM200_CAN_ID_TORQUE_CMD   0x0C0u
#define CM200_CAN_ID_FEEDBACK_1   0x0B0u   // commanded torque, torque fb, speed, bus V
#define CM200_CAN_ID_FEEDBACK_2   0x0ACu   // commanded torque, torque fb, time since ON

// Local CM200 state (optional telemetry storage)
typedef struct {
    FDCAN_HandleTypeDef *hfdcan;
    float last_torque_cmd_nm;
    float last_torque_fb_nm;
    int16_t last_rpm;
    uint16_t last_vbus;
    bool initialized;
} cm200_state_t;

static cm200_state_t s_cm200;

// Simple clamp helper
static float cm200_clamp_f(float x, float lo, float hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

void cm200_init(FDCAN_HandleTypeDef *hfdcan)
{
    if (hfdcan == NULL) {
        return;
    }

    s_cm200.hfdcan = hfdcan;
    s_cm200.last_torque_cmd_nm = 0.0f;
    s_cm200.last_torque_fb_nm  = 0.0f;
    s_cm200.last_rpm           = 0;
    s_cm200.last_vbus          = 0;
    s_cm200.initialized        = false;

    // Start FDCAN
    if (HAL_FDCAN_Start(s_cm200.hfdcan) != HAL_OK) {
        Error_Handler();
    }

    // Configure a simple "accept all" filter into RX FIFO0
    FDCAN_FilterTypeDef filter = {0};
    filter.IdType       = FDCAN_STANDARD_ID;
    filter.FilterIndex  = 0;
    filter.FilterType   = FDCAN_FILTER_MASK;
    filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    filter.FilterID1    = 0x000;
    filter.FilterID2    = 0x000;

    if (HAL_FDCAN_ConfigFilter(s_cm200.hfdcan, &filter) != HAL_OK) {
        Error_Handler();
    }

    s_cm200.initialized = true;
}

void cm200_send_torque(float torque_nm)
{
    if (!s_cm200.initialized || s_cm200.hfdcan == NULL) {
        return;
    }

    // For now, hard-code a safe max torque for motor spin.
    // This should match (or be <=) TORQUE_MAX_NM. (5 for motor spin)
    const float CM200_MAX_TORQUE_NM = 5.0f;

    torque_nm = cm200_clamp_f(torque_nm, 0.0f, CM200_MAX_TORQUE_NM);
    s_cm200.last_torque_cmd_nm = torque_nm;

    // Convert to int16 in 0.1 Nm units
    int16_t tq_cmd_i16 = (int16_t)lroundf(torque_nm * 10.0f);

    // RPM request: 0 = no speed control
    int16_t rpm_req_i16 = 0;

    // Torque limit: same as CM200_MAX_TORQUE_NM
    int16_t tq_limit_i16 = (int16_t)lroundf(CM200_MAX_TORQUE_NM * 10.0f);

    uint8_t data[8] = {0};

    // Little endian
    data[0] = (uint8_t)(tq_cmd_i16 & 0xFF);
    data[1] = (uint8_t)((tq_cmd_i16 >> 8) & 0xFF);

    data[2] = (uint8_t)(rpm_req_i16 & 0xFF);
    data[3] = (uint8_t)((rpm_req_i16 >> 8) & 0xFF);

    data[4] = 1u; // direction: 1 = forward
    data[5] = 1u; // enable inverter

    data[6] = (uint8_t)(tq_limit_i16 & 0xFF);
    data[7] = (uint8_t)((tq_limit_i16 >> 8) & 0xFF);

    FDCAN_TxHeaderTypeDef hdr = {0};
    hdr.Identifier          = CM200_CAN_ID_TORQUE_CMD;
    hdr.IdType              = FDCAN_STANDARD_ID;
    hdr.TxFrameType         = FDCAN_DATA_FRAME;
    hdr.FDFormat            = FDCAN_CLASSIC_CAN;
    hdr.BitRateSwitch       = FDCAN_BRS_OFF;
    hdr.DataLength          = FDCAN_DLC_BYTES_8;
    hdr.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    hdr.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;

    (void)HAL_FDCAN_AddMessageToTxFifoQ(s_cm200.hfdcan, &hdr, data);
}

void cm200_process_rx(void)
{
    if (!s_cm200.initialized || s_cm200.hfdcan == NULL) {
        return;
    }

    FDCAN_RxHeaderTypeDef rx;
    uint8_t data[8];

    while (HAL_FDCAN_GetRxFifoFillLevel(s_cm200.hfdcan, FDCAN_RX_FIFO0) > 0) {
        if (HAL_FDCAN_GetRxMessage(s_cm200.hfdcan, FDCAN_RX_FIFO0, &rx, data) != HAL_OK) {
            break;
        }

        if (rx.Identifier == CM200_CAN_ID_FEEDBACK_1) {
            // 0x0B0: commanded torque (int16, 0.1Nm), torque fb (int16, 0.1Nm),
            // motor speed (int16, 1rpm), bus voltage (uint16)
            int16_t tq_cmd_i16 = (int16_t)((data[1] << 8) | data[0]);
            int16_t tq_fb_i16  = (int16_t)((data[3] << 8) | data[2]);
            int16_t rpm_i16    = (int16_t)((data[5] << 8) | data[4]);
            uint16_t vbus_u16  = (uint16_t)((data[7] << 8) | data[6]);

            s_cm200.last_torque_cmd_nm = (float)tq_cmd_i16 / 10.0f;
            s_cm200.last_torque_fb_nm  = (float)tq_fb_i16 / 10.0f;
            s_cm200.last_rpm           = rpm_i16;
            s_cm200.last_vbus          = vbus_u16;

            log_printf(LOG_INFO,
                       "INV 0x0B0: tq_cmd=%.1f Nm tq_fb=%.1f Nm rpm=%d Vbus=%u\r\n",
                       s_cm200.last_torque_cmd_nm,
                       s_cm200.last_torque_fb_nm,
                       s_cm200.last_rpm,
                       s_cm200.last_vbus);
        } else if (rx.Identifier == CM200_CAN_ID_FEEDBACK_2) {
            // 0x0AC: commanded torque (int16, 0.1Nm), torque fb (int16, 0.1Nm),
            // time since ON (uint32)
            int16_t tq_cmd_i16 = (int16_t)((data[1] << 8) | data[0]);
            int16_t tq_fb_i16  = (int16_t)((data[3] << 8) | data[2]);
            uint32_t t_on_ms   = (uint32_t)data[4]
                               | ((uint32_t)data[5] << 8)
                               | ((uint32_t)data[6] << 16)
                               | ((uint32_t)data[7] << 24);

            s_cm200.last_torque_cmd_nm = (float)tq_cmd_i16 / 10.0f;
            s_cm200.last_torque_fb_nm  = (float)tq_fb_i16 / 10.0f;

            log_printf(LOG_INFO,
                       "INV 0x0AC: tq_cmd=%.1f Nm tq_fb=%.1f Nm t_on=%lu ms\r\n",
                       s_cm200.last_torque_cmd_nm,
                       s_cm200.last_torque_fb_nm,
                       (unsigned long)t_on_ms);
        }
    }
}
