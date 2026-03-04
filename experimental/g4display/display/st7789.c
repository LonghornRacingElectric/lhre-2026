#include "st7789.h"
#include "cmsis_os.h"
#include "spi.h"
#include <stm32g4xx_hal_spi.h>

void ST7789_WriteCommand(uint8_t cmd) {
  HAL_GPIO_WritePin(LCD_NCS_GPIO_Port, LCD_NCS_Pin,
                    GPIO_PIN_RESET); // CS Low to begin
  HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin,
                    GPIO_PIN_RESET); // DC Low for Command
  HAL_SPI_Transmit(&hspi2, &cmd, 1, HAL_MAX_DELAY);
  HAL_GPIO_WritePin(LCD_NCS_GPIO_Port, LCD_NCS_Pin,
                    GPIO_PIN_SET); // CS High to end
}

void ST7789_WriteData(uint8_t data) {
  HAL_GPIO_WritePin(LCD_NCS_GPIO_Port, LCD_NCS_Pin,
                    GPIO_PIN_RESET); // CS Low to begin
  HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin,
                    GPIO_PIN_SET); // DC High for Data
  HAL_SPI_Transmit(&hspi2, &data, 1, HAL_MAX_DELAY);
  HAL_GPIO_WritePin(LCD_NCS_GPIO_Port, LCD_NCS_Pin,
                    GPIO_PIN_SET); // CS High to end
}

void ST7789_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
  ST7789_WriteCommand(0x2A); // Column Address Set
  ST7789_WriteData(x0 >> 8);
  ST7789_WriteData(x0 & 0xFF);
  ST7789_WriteData(x1 >> 8);
  ST7789_WriteData(x1 & 0xFF);

  ST7789_WriteCommand(0x2B); // Row Address Set
  ST7789_WriteData(y0 >> 8);
  ST7789_WriteData(y0 & 0xFF);
  ST7789_WriteData(y1 >> 8);
  ST7789_WriteData(y1 & 0xFF);

  ST7789_WriteCommand(0x2C); // Memory Write
}

void ST7789_TestPattern(void) {
  // Clear screen to Black (0x0000)
  ST7789_SetWindow(0, 0, 239, 239);
  HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET);

  uint8_t black[2] = {0, 0};
  uint8_t white[2] = {0xFF, 0xFF};

  for (int y = 0; y < 240; y++) {
    for (int x = 0; x < 240; x++) {
      // Create a grid: white pixel every 20 pixels
      if (x % 20 == 0 || y % 20 == 0) {
        HAL_SPI_Transmit(&hspi2, white, 2, HAL_MAX_DELAY);
      } else {
        HAL_SPI_Transmit(&hspi2, black, 2, HAL_MAX_DELAY);
      }
    }
  }
}

void ST7789_Init(void) {
  // 3. MINIMAL INIT SEQUENCE
  ST7789_WriteCommand(0x01); // Software Reset
  osDelay(150);
  ST7789_WriteCommand(0x11); // Sleep Out
  osDelay(120);

  ST7789_WriteCommand(0x3A); // Interface Pixel Format
  ST7789_WriteData(0x05);    // 16-bit/pixel (RGB565)

  ST7789_WriteCommand(0x36); // MADCTL: Memory Data Access Control
  ST7789_WriteData(
      0x00); // 0x00 is default. Use 0x70, 0xC0 etc., to rotate screen

  ST7789_WriteCommand(
      0x21); // Display Inversion ON (Crucial for most ST7789 IPS panels)

  ST7789_WriteCommand(0x29); // Display ON
  osDelay(10);
}

void ST7789_DrawBitmap(uint16_t w, uint16_t h, const uint8_t *bitmap) {
  HAL_GPIO_WritePin(LCD_NCS_GPIO_Port, LCD_NCS_Pin,
                    GPIO_PIN_RESET); // CS Low
  HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin,
                    GPIO_PIN_SET); // DC High for data
  HAL_SPI_Transmit(&hspi2, (uint8_t *)bitmap, w * h * 2, HAL_MAX_DELAY);
  HAL_GPIO_WritePin(LCD_NCS_GPIO_Port, LCD_NCS_Pin,
                    GPIO_PIN_SET); // CS High
}

void ST7789_DrawBitmap_DMA(uint16_t w, uint16_t h, const uint8_t *bitmap) {
  HAL_GPIO_WritePin(LCD_NCS_GPIO_Port, LCD_NCS_Pin,
                    GPIO_PIN_RESET); // CS Low
  HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin,
                    GPIO_PIN_SET); // DC High for data
  /* Non-blocking DMA transfer. CS is released in HAL_SPI_TxCpltCallback
   * (implemented in LCDController.c). */
  HAL_SPI_Transmit_DMA(&hspi2, (uint8_t *)bitmap, w * h * 2);
}