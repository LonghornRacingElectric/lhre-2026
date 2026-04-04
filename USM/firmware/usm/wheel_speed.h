#ifndef WHEEL_SPEED_H
#define WHEEL_SPEED_H

#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_spi.h"
#include <stdint.h>

// ── Config ────────────────────────────────────────────────
#define WS_NUM_SENSORS        4
#define WS_MAGNETS_PER_REV    4      //4 magnets on wheel
#define WS_SENSOR_SPACING_DEG 15.0f  //sensors 15° apart
#define WS_POLL_RATE_MS       10     //poll each sensor every 10ms (100Hz)
#define WS_TIMEOUT_MS         500    //zero RPM if no detection for 500ms
#define WS_PEAK_THRESHOLD     1000   //minimum field magnitude to consider a peak
#define WS_PEAK_DROP_RATIO    0.7f   //trigger detection when field drops to 70% of peak

// ── CS Pin definitions ────────────────────────────────────
#define HE_CS1_PORT   GPIOB
#define HE_CS1_PIN    GPIO_PIN_12
#define HE_CS2_PORT   GPIOB
#define HE_CS2_PIN    GPIO_PIN_11
#define HE_CS3_PORT   GPIOA
#define HE_CS3_PIN    GPIO_PIN_5
#define HE_CS4_PORT   GPIOA
#define HE_CS4_PIN    GPIO_PIN_6

// ── Public API ────────────────────────────────────────────
void  WheelSpeed_Init(SPI_HandleTypeDef *hspi);
void  WheelSpeed_Update(void);
float WheelSpeed_GetRPM(void);
float WheelSpeed_GetMPH(void);
float WheelSpeed_GetMagnitude(int sensor_idx);

#endif // WHEEL_SPEED_H
