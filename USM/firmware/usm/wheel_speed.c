#include "wheel_speed.h"
#include "wheel_calibration.h"
#include "mlx90395_frame.h"
#include "orion_time_us.h"
#include <string.h>
#include <math.h>
#include <longhorn/rtos/logger.h>
#include "cmsis_os.h"

// ── MLX90395 Commands ─────────────────────────────────────
#define MLX_CMD_START_BURST   0x1E
#define MLX_CMD_READ_MEAS     0x40

// ── Internal sensor state ─────────────────────────────────
typedef struct {
    GPIO_TypeDef *cs_port;
    uint16_t      cs_pin;

    uint8_t     id;
    bool        initialized;
    bool        counter_seen;
    uint8_t     counter;
    float       x, y, z;
    float       magnitude;
    int8_t      direction;
    float       last_tick;
    float       speed_rad_s;
} SensorState;

static SPI_HandleTypeDef *_hspi;
static SensorState _sensors[WS_NUM_SENSORS];
static float _wheel_speed_rad_s = 0.0f;
static wheel_phase_t phase_observer;
static wheel_phase_sample_t phase_sample;
static uint8_t phase_sequence;

// ── CS helpers ────────────────────────────────────────────
static void cs_low(SensorState *s) {
    HAL_GPIO_WritePin(s->cs_port, s->cs_pin, GPIO_PIN_RESET);
}
static void cs_high(SensorState *s) {
    HAL_GPIO_WritePin(s->cs_port, s->cs_pin, GPIO_PIN_SET);
}

// ── Start burst mode ──────────────────────────────────────
static bool mlx_start_burst(SensorState *s)
{
    uint8_t cmd = MLX_CMD_START_BURST;
    uint8_t status = 0;
    cs_low(s);
    HAL_Delay(15);
    HAL_StatusTypeDef sent = HAL_SPI_Transmit(_hspi, &cmd, 1, 2);
    HAL_StatusTypeDef received = sent == HAL_OK
        ? HAL_SPI_Receive(_hspi, &status, 1, 2) : HAL_ERROR;
    cs_high(s);
    HAL_Delay(15);
    return received == HAL_OK && (status & 0x80u) != 0u &&
           (status & 0x0cu) == 0u;
}

// ── Read X/Y/Z from one sensor ────────────────────────────
static bool mlx_read(SensorState *s)
{
    if (!s->initialized) return false;
    uint8_t cmd = MLX_CMD_READ_MEAS;
    uint8_t rx[12] = {0};
    cs_low(s);
    HAL_StatusTypeDef sent = HAL_SPI_Transmit(_hspi, &cmd, 1, 2);
    HAL_StatusTypeDef received = sent == HAL_OK
        ? HAL_SPI_Receive(_hspi, rx, sizeof(rx), 2) : HAL_ERROR;
    uint32_t read_time = orion_time_us();
    cs_high(s);
    mlx90395_frame_t frame;
    if (received != HAL_OK || !mlx90395_decode_frame(rx, &frame)) return false;
    if (frame.sensor_reset) {
        wheel_phase_reset(&phase_observer);
        s->counter_seen = false;
        return false;
    }
    /* Conservative: repeated 3-bit counter cannot prove a new conversion.
     * A gap of exactly eight conversions also requires reacquisition. */
    if (!frame.fresh || (s->counter_seen && frame.counter == s->counter))
        return false;
    s->counter = frame.counter;
    s->counter_seen = true;
    float raw[3];
    for (unsigned i = 0; i < 3; ++i) raw[i] = frame.axis[i] * 0.00714f;
#if defined(BOARD_RL) || defined(BOARD_RR)
    phase_sample.radial[s->id] = raw[0];
#else
    phase_sample.radial[s->id] = raw[s->id == 0 ? 0 : 1];
#endif
    /* This correction must be characterized before timing is qualified. */
    phase_sample.sample_time_us[s->id] = read_time - ORION_WHEEL_CONVERSION_AGE_US;
    phase_sample.valid_mask |= (uint8_t)(1u << s->id);
    phase_sample.fresh_mask |= (uint8_t)(1u << s->id);
    /* Preserve the legacy threshold estimator for existing DAQ telemetry. */
    s->x = raw[0] * WS_EMA_ALPHA + s->x * (1.0f - WS_EMA_ALPHA);
    s->y = raw[1] * WS_EMA_ALPHA + s->y * (1.0f - WS_EMA_ALPHA);
    s->z = raw[2] * WS_EMA_ALPHA + s->z * (1.0f - WS_EMA_ALPHA);
    return true;
}

// ── Init ──────────────────────────────────────────────────
void WheelSpeed_Init(SPI_HandleTypeDef *hspi)
{
    _hspi = hspi;

    _sensors[0].cs_port = HE_CS1_PORT;
    _sensors[0].cs_pin  = HE_CS1_PIN;
    _sensors[1].cs_port = HE_CS2_PORT;
    _sensors[1].cs_pin  = HE_CS2_PIN;
    _sensors[2].cs_port = HE_CS3_PORT;
    _sensors[2].cs_pin  = HE_CS3_PIN;
    _sensors[3].cs_port = HE_CS4_PORT;
    _sensors[3].cs_pin  = HE_CS4_PIN;

    /* Every device on SPI2 must be deselected before the first command. */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_SET);
    for (int i = 0; i < WS_NUM_SENSORS; i++) cs_high(&_sensors[i]);
    wheel_phase_config_t calibration = orion_wheel_calibration();
    wheel_phase_init(&phase_observer, &calibration);
    for (int i = 0; i < WS_NUM_SENSORS; i++) {
        cs_high(&_sensors[i]);
        _sensors[i].direction = 1;
        _sensors[i].id = i;
        _sensors[i].initialized = mlx_start_burst(&_sensors[i]);
    }
}

// ── Peak detection + RPM math ─────────────────────────────
static void process_sensor(SensorState *s)
{
    
    float value;
#if defined(BOARD_RL) || defined(BOARD_RR)
    // all are X-radial are rotated on rears
    value = s->x;
#else
    // first device is X-radial on fronts, otherwise Y-radial
    if(s->id == 0) value = s->x;
    else value = s->y;
#endif

    uint8_t cross = 0;
    if(s->direction == 1) {
        if(value > WS_HYSTERESIS) {
            s->direction = -1;
            cross = 1;
        }
    } else { // -1
        if(value < -WS_HYSTERESIS) {
            s->direction = 1;
            cross = 1;
        }
    }
    
    float now = osKernelGetTickCount() * 0.001f;
    float elapsed = now - s->last_tick;
    if (elapsed <= 0.0f) return; // Never publish divide-by-zero legacy speed.
    float hypothetical_speed_rad_s = (2.0f * 3.14159f) / (elapsed * WS_MAGNETS_PER_REV);

    if(cross) {
        s->last_tick = now;
        s->speed_rad_s = hypothetical_speed_rad_s;
    } else if(hypothetical_speed_rad_s < s->speed_rad_s) {
        // detect slowdown
        s->speed_rad_s = hypothetical_speed_rad_s;
    }
}

// ── Public: call from FreeRTOS task every WS_POLL_RATE_MS ─
void WheelSpeed_Update(void)
{
    phase_sample.valid_mask = 0;
    phase_sample.fresh_mask = 0;
    for (int i = 0; i < WS_NUM_SENSORS; i++) {
        if (mlx_read(&_sensors[i])) process_sensor(&_sensors[i]);
        if(i==0) {
            // log_printf(LOG_INFO, "X: %.2f | Y: %.2f | Z: %.2f | Last: %.3f | Speed: %.2f rad/s\r\n",
            //     _sensors[i].x, _sensors[i].y, _sensors[i].z, _sensors[i].last_tick, _sensors[i].speed_rad_s);
        }
    }

    wheel_phase_update(&phase_observer, &phase_sample, orion_time_us());
    if (phase_sample.fresh_mask == WHEEL_PHASE_ALL_CHANNELS) ++phase_sequence;

    float latest_tick = 0.0f;
    for (int i = 0; i < WS_NUM_SENSORS; i++) {
        if(_sensors[i].last_tick > latest_tick) {
            latest_tick = _sensors[i].last_tick;
            _wheel_speed_rad_s = _sensors[i].speed_rad_s;
        }
    }
    if(_wheel_speed_rad_s < 0.3f) {
        _wheel_speed_rad_s = 0.0f;
    }


}

// ── Public getters ────────────────────────────────────────


float WheelSpeed_GetSpeed()
{
    return _wheel_speed_rad_s;
}

const wheel_phase_output_t *WheelSpeed_GetPhaseEstimate(void) {
    return &phase_observer.output;
}
uint8_t WheelSpeed_GetPhaseSequence(void) { return phase_sequence; }
