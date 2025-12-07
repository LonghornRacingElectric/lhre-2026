#include "inverter_cm200.h"
#include "longhorn/can_base.h"
#include "longhorn/rtos/logger.h"

#include <math.h>
#include <stdbool.h>
#include <string.h>

// CAN IDs for CM200DZ based on 2025 data
#define CM200_CAN_ID_TORQUE_CMD   0x0C0u  // VCU -> Inverter torque command
#define CM200_CAN_ID_FEEDBACK_1   0x0B0u  // commanded torque, torque fb, speed, bus V
#define CM200_CAN_ID_FEEDBACK_2   0x0ACu  // commanded torque, torque fb, time since ON

// Enable fake inverter simulation
#define CM200_SIM_MODE   0

// Payload for torque command frame (0x0C0)
typedef struct {
    float   torque_cmd_nm;
    float   torque_limit_nm;
    int16_t rpm_req;
    uint8_t direction;
    uint8_t enable;
} cm200_torque_cmd_msg_t;

// Decoded feedback from 0x0B0
typedef struct {
    float   tq_cmd_nm;
    float   tq_fb_nm;
    int16_t rpm;
    uint16_t vbus_decivolts;
} cm200_fb1_msg_t;

// Decoded feedback from 0x0AC
typedef struct {
    float   tq_cmd_nm;
    float   tq_fb_nm;
    uint32_t time_on_ms;
} cm200_fb2_msg_t;

typedef struct {
    can_interface_t    can_if;
    can_message_t*     tx_msg;
    cm200_torque_cmd_msg_t tx_payload;

    cm200_fb1_msg_t    fb1_payload;
    cm200_fb2_msg_t    fb2_payload;
    can_receive_message_t* fb1_handle;
    can_receive_message_t* fb2_handle;

    float   last_torque_cmd_nm;
    float   last_torque_fb_nm;
    int16_t last_rpm;
    uint16_t last_vbus;
    bool    initialized;
} cm200_state_t;

static cm200_state_t s_cm200;

// Clamp helper
static float cm200_clamp_f(float x, float lo, float hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

// Pack VCU -> Inverter torque command (0x0C0)
static int cm200_torque_cmd_pack(const void* msg_ptr, uint8_t* dst)
{
    const cm200_torque_cmd_msg_t* msg = (const cm200_torque_cmd_msg_t*)msg_ptr;

    float tq_cmd_nm    = msg->torque_cmd_nm;
    float tq_limit_nm  = msg->torque_limit_nm;
    int16_t rpm_req    = msg->rpm_req;

    int16_t tq_cmd_i16   = (int16_t)lroundf(tq_cmd_nm * 10.0f);       // 0.1 Nm
    int16_t tq_limit_i16 = (int16_t)lroundf(tq_limit_nm * 10.0f);     // 0.1 Nm

    dst[0] = (uint8_t)(tq_cmd_i16 & 0xFF);
    dst[1] = (uint8_t)((tq_cmd_i16 >> 8) & 0xFF);

    dst[2] = (uint8_t)(rpm_req & 0xFF);
    dst[3] = (uint8_t)((rpm_req >> 8) & 0xFF);

    dst[4] = msg->direction;   // 1 = forward
    dst[5] = msg->enable;      // 1 = enable inverter

    dst[6] = (uint8_t)(tq_limit_i16 & 0xFF);
    dst[7] = (uint8_t)((tq_limit_i16 >> 8) & 0xFF);

    return 8;
}

// Unpack Inverter -> VCU feedback 0x0B0
// torque_cmd (0.1Nm), torque_fb (0.1Nm), speed (rpm), Vbus (0.1V)
static int cm200_fb1_unpack(uint8_t* data, const void* msg_ptr)
{
    cm200_fb1_msg_t* msg = (cm200_fb1_msg_t*)msg_ptr;

    int16_t tq_cmd_i16 = (int16_t)((data[1] << 8) | data[0]);
    int16_t tq_fb_i16  = (int16_t)((data[3] << 8) | data[2]);
    int16_t rpm_i16    = (int16_t)((data[5] << 8) | data[4]);
    uint16_t vbus_u16  = (uint16_t)((data[7] << 8) | data[6]);

    msg->tq_cmd_nm        = (float)tq_cmd_i16 / 10.0f;
    msg->tq_fb_nm         = (float)tq_fb_i16 / 10.0f;
    msg->rpm              = rpm_i16;
    msg->vbus_decivolts   = vbus_u16;

    return 0;
}

// Unpack Inverter -> VCU feedback 0x0AC
// torque_cmd (0.1Nm), torque_fb (0.1Nm), time since ON (ms)
static int cm200_fb2_unpack(uint8_t* data, const void* msg_ptr)
{
    cm200_fb2_msg_t* msg = (cm200_fb2_msg_t*)msg_ptr;

    int16_t tq_cmd_i16 = (int16_t)((data[1] << 8) | data[0]);
    int16_t tq_fb_i16  = (int16_t)((data[3] << 8) | data[2]);
    uint32_t t_on_ms   = (uint32_t)data[4]
                       | ((uint32_t)data[5] << 8)
                       | ((uint32_t)data[6] << 16)
                       | ((uint32_t)data[7] << 24);

    msg->tq_cmd_nm   = (float)tq_cmd_i16 / 10.0f;
    msg->tq_fb_nm    = (float)tq_fb_i16 / 10.0f;
    msg->time_on_ms  = t_on_ms;

    return 0;
}

void cm200_init(FDCAN_HandleTypeDef* hfdcan)
{
    if (hfdcan == NULL) {
        return;
    }

    memset(&s_cm200, 0, sizeof(s_cm200));

    // Attach our interface to this FDCAN handle
    memset(&s_cm200.can_if, 0, sizeof(s_cm200.can_if));
    s_cm200.can_if.handle = hfdcan;

#if CM200_SIM_MODE == 0
    // Register interface with CAN library (starts FDCAN, enables IRQs, sets up callbacks)
    can_register_interface(&s_cm200.can_if);

    // Create TX message handle for 0x0C0
    s_cm200.tx_payload.torque_cmd_nm   = 0.0f;
    s_cm200.tx_payload.torque_limit_nm = 5.0f;
    s_cm200.tx_payload.rpm_req         = 0;
    s_cm200.tx_payload.direction       = 1u;
    s_cm200.tx_payload.enable          = 1u;

    s_cm200.tx_msg = can_get_message_handle(
        &s_cm200.tx_payload,
        CM200_CAN_ID_TORQUE_CMD,
        0,                       // no periodic send, we call can_send_immediate
        FDCAN_DLC_BYTES_8,
        cm200_torque_cmd_pack
    );

    if (s_cm200.tx_msg != NULL) {
        s_cm200.tx_msg->id_type = FDCAN_STANDARD_ID;
    }

    // Register feedback 0x0B0
    s_cm200.fb1_handle = can_get_receive_message_handle(
        &s_cm200.fb1_payload,
        CM200_CAN_ID_FEEDBACK_1,
        cm200_fb1_unpack
    );
    if (s_cm200.fb1_handle != NULL) {
        can_register_receive_packet(&s_cm200.can_if, s_cm200.fb1_handle);
    }

    // Register feedback 0x0AC
    s_cm200.fb2_handle = can_get_receive_message_handle(
        &s_cm200.fb2_payload,
        CM200_CAN_ID_FEEDBACK_2,
        cm200_fb2_unpack
    );
    if (s_cm200.fb2_handle != NULL) {
        can_register_receive_packet(&s_cm200.can_if, s_cm200.fb2_handle);
    }
#endif

    s_cm200.last_torque_cmd_nm = 0.0f;
    s_cm200.last_torque_fb_nm  = 0.0f;
    s_cm200.last_rpm           = 0;
    s_cm200.last_vbus          = 0;
    s_cm200.initialized        = true;
}

void cm200_send_torque(float torque_nm)
{
    if (!s_cm200.initialized) {
        return;
    }

    const float CM200_MAX_TORQUE_NM = 5.0f;
    torque_nm = cm200_clamp_f(torque_nm, 0.0f, CM200_MAX_TORQUE_NM);
    s_cm200.last_torque_cmd_nm = torque_nm;

#if CM200_SIM_MODE == 1
    // No real CAN, just log
    log_printf(LOG_INFO, "[SIM] TX torque = %.2f Nm\r\n", torque_nm);
    return;
#else
    if (s_cm200.tx_msg == NULL) {
        return;
    }

    s_cm200.tx_payload.torque_cmd_nm   = torque_nm;
    s_cm200.tx_payload.torque_limit_nm = CM200_MAX_TORQUE_NM;
    s_cm200.tx_payload.rpm_req         = 0;
    s_cm200.tx_payload.direction       = 1u;
    s_cm200.tx_payload.enable          = 1u;

    can_send_immediate(&s_cm200.can_if, s_cm200.tx_msg);
#endif
}

void cm200_process_rx(void)
{
    if (!s_cm200.initialized) {
        return;
    }

#if CM200_SIM_MODE == 1
    // Fake inverter response based on last commanded torque
    s_cm200.last_torque_fb_nm = s_cm200.last_torque_cmd_nm;
    s_cm200.last_rpm = (int16_t)(s_cm200.last_torque_cmd_nm * 1000.0f);
    s_cm200.last_vbus = 3500; // 350.0 V in 0.1 V units

    log_printf(LOG_INFO,
        "[SIM] INV: tq_cmd=%.1f Nm tq_fb=%.1f Nm rpm=%d Vbus=%u\r\n",
        s_cm200.last_torque_cmd_nm,
        s_cm200.last_torque_fb_nm,
        s_cm200.last_rpm,
        s_cm200.last_vbus
    );
#else
    // Use latest decoded feedback from 0x0B0 / 0x0AC
    s_cm200.last_torque_cmd_nm = s_cm200.fb1_payload.tq_cmd_nm;
    s_cm200.last_torque_fb_nm  = s_cm200.fb1_payload.tq_fb_nm;
    s_cm200.last_rpm           = s_cm200.fb1_payload.rpm;
    s_cm200.last_vbus          = s_cm200.fb1_payload.vbus_decivolts;

    log_printf(LOG_INFO,
        "INV 0x0B0: tq_cmd=%.1f Nm tq_fb=%.1f Nm rpm=%d Vbus=%u (0.1V)\r\n",
        s_cm200.last_torque_cmd_nm,
        s_cm200.last_torque_fb_nm,
        s_cm200.last_rpm,
        s_cm200.last_vbus
    );

    log_printf(LOG_INFO,
        "INV 0x0AC: tq_cmd=%.1f Nm tq_fb=%.1f Nm t_on=%lu ms\r\n",
        s_cm200.fb2_payload.tq_cmd_nm,
        s_cm200.fb2_payload.tq_fb_nm,
        (unsigned long)s_cm200.fb2_payload.time_on_ms
    );
#endif
}
