//
// Created by Gautham Ramanarayanan on 3/20/25. Modified by Arav Karnik on 3/30/2026
//

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include "main.h"

#define BOARD_NAME                  "Corner Sensor Module"

// Board SPI config
// Slave 1 on SPI1
#define S2PI_SLAVE1                 1

#ifndef SPI_MAX_BAUD_RATE
#define SPI_MAX_BAUD_RATE           25000000
#endif

#ifndef SPI_DEFAULT_SLAVE
#define SPI_DEFAULT_SLAVE           (S2PI_SLAVE1)
#endif

// SPI1 Pins (from CSM schematic)
#define S2PI_CLK_PORT               GPIOA
#define S2PI_CLK_PIN                GPIO_PIN_5
#define S2PI_CS_PORT                GPIOB
#define S2PI_CS_PIN                 GPIO_PIN_0
#define S2PI_CS_PORTCLK_ENB()       __HAL_RCC_GPIOB_CLK_ENABLE()
#define S2PI_MOSI_PORT              GPIOA
#define S2PI_MOSI_PIN               GPIO_PIN_7
#define S2PI_MISO_PORT              GPIOA
#define S2PI_MISO_PIN               GPIO_PIN_6
#define S2PI_IRQ_PORT               GPIOB
#define S2PI_IRQ_PIN                GPIO_PIN_15
#define S2PI_IRQ_EXTI               EXTI15_10_IRQn
#define S2PI_IRQ_PORTCLK_ENB()      __HAL_RCC_GPIOB_CLK_ENABLE()

#endif //BOARD_CONFIG_H