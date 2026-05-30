#include "wheel_speed.h"
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
    float       x, y, z;
    float       magnitude;
    int8_t      direction;
    float       last_tick;
    float       speed_rad_s;
} SensorState;

static SPI_HandleTypeDef *_hspi;
static SensorState _sensors[WS_NUM_SENSORS];
static float _wheel_speed_rad_s = 0.0f;

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
    s->x = ((int16_t)((rx[2] << 8) | rx[3])) * 0.00714f * WS_EMA_ALPHA + s->x * (1.0f - WS_EMA_ALPHA);
    s->y = ((int16_t)((rx[4] << 8) | rx[5])) * 0.00714f * WS_EMA_ALPHA + s->y * (1.0f - WS_EMA_ALPHA);
    s->z = ((int16_t)((rx[6] << 8) | rx[7])) * 0.00714f * WS_EMA_ALPHA + s->z * (1.0f - WS_EMA_ALPHA);
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
        _sensors[i].direction = 1;
        _sensors[i].id = i;
        mlx_start_burst(&_sensors[i]);
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
    for (int i = 0; i < WS_NUM_SENSORS; i++) {
        mlx_read(&_sensors[i]);
        process_sensor(&_sensors[i]);
        if(i==0) {
            // log_printf(LOG_INFO, "X: %.2f | Y: %.2f | Z: %.2f | Last: %.3f | Speed: %.2f rad/s\r\n",
            //     _sensors[i].x, _sensors[i].y, _sensors[i].z, _sensors[i].last_tick, _sensors[i].speed_rad_s);
        }
    }

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

    static int count = 0;
    count++;
    if(count == 10) {
        count = 0;
        log_printf(LOG_INFO, "Speeds (rad/s) || S1: %.2f | S2: %.2f | S3: %.2f | S4: %.2f || S: %.2f \r\n",
            _sensors[0].speed_rad_s, _sensors[1].speed_rad_s, _sensors[2].speed_rad_s, _sensors[3].speed_rad_s, _wheel_speed_rad_s);
    }
}

// ── Public getters ────────────────────────────────────────


float WheelSpeed_GetSpeed()
{
    return _wheel_speed_rad_s;
}
