#ifndef WHEEL_SPEED_H
#define WHEEL_SPEED_H

#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_spi.h"
#include <stdint.h>

// ── Config ────────────────────────────────────────────────
#define WS_NUM_SENSORS        4
#define WS_MAGNETS_PER_REV    6      //6 magnets on wheel
#define WS_SENSOR_SPACING_DEG 15.0f  //sensors 15° apart
#define WS_HYSTERESIS         3.0f
#define WS_EMA_ALPHA          0.05f

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
float WheelSpeed_GetSpeed();

#endif // WHEEL_SPEED_H
