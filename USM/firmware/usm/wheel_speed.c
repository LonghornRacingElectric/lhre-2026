#include "wheel_speed.h"
#include <string.h>
#include <math.h>
#include <longhorn/rtos/logger.h>

// ── MLX90395 Commands ─────────────────────────────────────
#define MLX_CMD_START_BURST   0x1E
#define MLX_CMD_READ_MEAS     0x40

// ── Internal sensor state ─────────────────────────────────
typedef struct {
    GPIO_TypeDef *cs_port;
    uint16_t      cs_pin;
    int16_t       x, y, z;
    float         magnitude;
    float         peak_magnitude;
    uint8_t       magnet_detected;
    uint32_t      last_trigger_tick;
} SensorState;

static SPI_HandleTypeDef *_hspi;
static SensorState _sensors[WS_NUM_SENSORS];
static float _wheel_rpm = 0.0f;

// ── CS helpers ────────────────────────────────────────────
static void cs_low(SensorState *s) {
    HAL_GPIO_WritePin(s->cs_port, s->cs_pin, GPIO_PIN_RESET);
}
static void cs_high(SensorState *s) {
    HAL_GPIO_WritePin(s->cs_port, s->cs_pin, GPIO_PIN_SET);
}

// ── Start burst mode ──────────────────────────────────────
static void mlx_start_burst(SensorState *s)
{
    uint8_t cmd = MLX_CMD_START_BURST;
    uint8_t status;
    cs_low(s);
    HAL_Delay(15);
    HAL_SPI_Transmit(_hspi, &cmd, 1, HAL_MAX_DELAY);
    HAL_SPI_Receive(_hspi, &status, 1, HAL_MAX_DELAY);
    cs_high(s);
    HAL_Delay(15);
}

// ── Read X/Y/Z from one sensor ────────────────────────────
static void mlx_read(SensorState *s)
{
    uint8_t cmd = MLX_CMD_READ_MEAS;
    uint8_t rx[9] = {0};

    cs_low(s);
    HAL_SPI_Transmit(_hspi, &cmd, 1, HAL_MAX_DELAY);
    HAL_SPI_Receive(_hspi, rx, 9, HAL_MAX_DELAY);
    cs_high(s);

    // bytes: [0]=status, [1]=crc, [2-3]=X, [4-5]=Y, [6-7]=Z, [8]=T
    s->x = (int16_t)((rx[2] << 8) | rx[3]);
    s->y = (int16_t)((rx[4] << 8) | rx[5]);
    s->z = (int16_t)((rx[6] << 8) | rx[7]);
    s->magnitude = sqrtf((float)(s->x * s->x) +
                         (float)(s->y * s->y) +
                         (float)(s->z * s->z));
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

    for (int i = 0; i < WS_NUM_SENSORS; i++) {
        cs_high(&_sensors[i]);
        _sensors[i].peak_magnitude    = 0.0f;
        _sensors[i].magnet_detected   = 0;
        _sensors[i].last_trigger_tick = 0;
        mlx_start_burst(&_sensors[i]);
    }
}

// ── Peak detection + RPM math ─────────────────────────────
static void process_sensor(SensorState *s)
{
    if (s->magnitude > s->peak_magnitude) {
        s->peak_magnitude = s->magnitude;
        s->magnet_detected = 0;
    }

    if (!s->magnet_detected &&
        s->peak_magnitude > WS_PEAK_THRESHOLD &&
        s->magnitude < s->peak_magnitude * WS_PEAK_DROP_RATIO)
    {
        uint32_t now = HAL_GetTick();

        if (s->last_trigger_tick != 0) {
            uint32_t delta_ms = now - s->last_trigger_tick;
            if (delta_ms > 5 && delta_ms < 2000) {
                float rpm = 60000.0f / ((float)delta_ms * 4.0f);
                _wheel_rpm = _wheel_rpm * 0.6f + rpm * 0.4f;
            }
        }

        s->last_trigger_tick = now;
        s->magnet_detected   = 1;
        s->peak_magnitude    = 0.0f;
    }
}

// ── Public: call from FreeRTOS task every WS_POLL_RATE_MS ─
void WheelSpeed_Update(void)
{
    static int count = 0;
    for (int i = 0; i < WS_NUM_SENSORS; i++) {
        mlx_read(&_sensors[i]);
        process_sensor(&_sensors[i]);
        if(i==0) {
            count++;
            if(count == 100) {
                count = 0;
                log_printf(LOG_INFO, "Magnitude: %.5f\n", _sensors[i].magnitude);
            }
        }
    }

    // If no sensor has triggered in WS_TIMEOUT_MS, decay RPM to zero
    uint32_t now = HAL_GetTick();
    uint32_t most_recent = 0;
    for (int i = 0; i < WS_NUM_SENSORS; i++) {
        if (_sensors[i].last_trigger_tick > most_recent)
            most_recent = _sensors[i].last_trigger_tick;
    }
    if (most_recent != 0 && (now - most_recent) > WS_TIMEOUT_MS) {
        _wheel_rpm = 0.0f;
    }
}

// ── Public getters ────────────────────────────────────────
float WheelSpeed_GetRPM(void)
{
    return _wheel_rpm;
}

float WheelSpeed_GetMagnitude(int sensor_idx)
{
    if (sensor_idx < 0 || sensor_idx >= WS_NUM_SENSORS) return 0.0f;
    return _sensors[sensor_idx].magnitude;
}

float WheelSpeed_GetMPH(void)
{
    return _wheel_rpm * 1.276f * 60.0f / 1609.34f; //1.276m = 16" circumference
}
