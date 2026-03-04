// #include "stm32l4xx_hal.h"
// #include "main.h"
// // SPI handle (assumed to be SPI1, configured elsewhere)
// extern SPI_HandleTypeDef hspi2;
//
// // ------ Define display control pins (adjust these as needed) -----------
// #define ST7796_CS_GPIO_Port   GPIOA
// #define ST7796_CS_Pin         GPIO_PIN_4
//
// #define ST7796_DC_GPIO_Port   GPIOB
// #define ST7796_DC_Pin         GPIO_PIN_15
//
// #define ST7796_RST_GPIO_Port  GPIOC
// #define ST7796_RST_Pin        GPIO_PIN_8
//
// // ------ Helper macros for controlling the pins ---------------------------
// #define ST7796_CS_LOW()   HAL_GPIO_WritePin(ST7796_CS_GPIO_Port, ST7796_CS_Pin, GPIO_PIN_RESET)
// #define ST7796_CS_HIGH()  HAL_GPIO_WritePin(ST7796_CS_GPIO_Port, ST7796_CS_Pin, GPIO_PIN_SET)
// #define ST7796_DC_CMD()   HAL_GPIO_WritePin(ST7796_DC_GPIO_Port, ST7796_DC_Pin, GPIO_PIN_RESET)
// #define ST7796_DC_DATA()  HAL_GPIO_WritePin(ST7796_DC_GPIO_Port, ST7796_DC_Pin, GPIO_PIN_SET)
// #define ST7796_RST_LOW()  HAL_GPIO_WritePin(ST7796_RST_GPIO_Port, ST7796_RST_Pin, GPIO_PIN_RESET)
// #define ST7796_RST_HIGH() HAL_GPIO_WritePin(ST7796_RST_GPIO_Port, ST7796_RST_Pin, GPIO_PIN_SET)
// #define ST7796_Delay(ms)  HAL_Delay(ms)
//
// // ------ SPI transmit helper functions ------------------------------------
// static void ST7796_SendCommand(uint8_t cmd) {
//     ST7796_DC_CMD();           // Command mode
//     ST7796_CS_LOW();
//     HAL_SPI_Transmit(&hspi2, &cmd, 1, HAL_MAX_DELAY);
//     ST7796_CS_HIGH();
// }
//
// static void ST7796_SendData(uint8_t *data, uint16_t len) {
//     ST7796_DC_DATA();          // Data mode
//     ST7796_CS_LOW();
//     HAL_SPI_Transmit(&hspi2, data, len, HAL_MAX_DELAY);
//     ST7796_CS_HIGH();
// }
//
// // Send a 16-bit value (for pixel data) as two bytes (big-endian)
// static void ST7796_SendData16(uint16_t data) {
//     uint8_t d[2];
//     d[0] = data >> 8;
//     d[1] = data & 0xFF;
//     ST7796_SendData(d, 2);
// }
//
// // ------ Set the drawing window on the display -----------------------------
// static void ST7796_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
//     uint8_t data[4];
//
//     // Column address set (command 0x2A)
//     ST7796_SendCommand(0x2A);
//     data[0] = x0 >> 8;
//     data[1] = x0 & 0xFF;
//     data[2] = x1 >> 8;
//     data[3] = x1 & 0xFF;
//     ST7796_SendData(data, 4);
//
//     // Row address set (command 0x2B)
//     ST7796_SendCommand(0x2B);
//     data[0] = y0 >> 8;
//     data[1] = y0 & 0xFF;
//     data[2] = y1 >> 8;
//     data[3] = y1 & 0xFF;
//     ST7796_SendData(data, 4);
//
//     // Prepare for memory write (command 0x2C)
//     ST7796_SendCommand(0x2C);
// }
//
// // ------ Initialization function for the ST7796 display ------------------
// void ST7796_Init(void) {
//     // Ensure CS, DC and RST start high
//     ST7796_CS_HIGH();
//     ST7796_DC_DATA();
//     ST7796_RST_HIGH();
//     ST7796_Delay(50);
//
//     // Reset the display hardware
//     ST7796_RST_LOW();
//     ST7796_Delay(50);
//     ST7796_RST_HIGH();
//     ST7796_Delay(120);
//
//     // Software reset
//     ST7796_SendCommand(0x01); // SWRESET
//     ST7796_Delay(150);
//
//     // Exit sleep mode
//     ST7796_SendCommand(0x11); // SLPOUT
//     ST7796_Delay(150);
//
//     // Set pixel format to 16-bit (5-6-5)
//     // For ST7796, CMD 0x3A with parameter 0x55 is common for 16-bit color.
//     uint8_t colmod = 0x55;
//     ST7796_SendCommand(0x3A); // COLMOD: Pixel Format Set
//     ST7796_SendData(&colmod, 1);
//     ST7796_Delay(10);
//
//     // Memory Data Access Control (MADCTL) – adjust orientation if needed.
//     // Here we send 0x00 for default orientation. Adjust this value if you want rotation.
//     uint8_t madctl = 0x00;
//     ST7796_SendCommand(0x36); // MADCTL
//     ST7796_SendData(&madctl, 1);
//
//     // Optional: Turn on display inversion if desired.
//     ST7796_SendCommand(0x21); // INVON (Display Inversion On)
//     ST7796_Delay(10);
//
//     // Finally, turn the display on.
//     ST7796_SendCommand(0x29); // DISPON (Display On)
//     ST7796_Delay(150);
// }
//
// // ------ Function to draw a blue square on a 480x320 display ---------------
// void Draw_Blue_Square(void) {
//     // Define square dimensions. For example, draw a 100x100 square centered on 480x320.
//     const uint16_t square_size = 100;
//     const uint16_t x0 = (480 - square_size) / 2; // 190
//     const uint16_t y0 = (320 - square_size) / 2; // 110
//     const uint16_t x1 = x0 + square_size - 1;
//     const uint16_t y1 = y0 + square_size - 1;
//
//     // Set the drawing area to the square region.
//     ST7796_SetAddressWindow(x0, y0, x1, y1);
//
//     // In RGB565, full blue is 0x001F (R=0, G=0, B=31).
//     const uint16_t blue = 0x0000;
//     uint32_t total_pixels = square_size * square_size;
//
//     // It is efficient to fill the area in chunks.
//     // Here we fill a small buffer (of 100 pixels) repeatedly.
//     #define CHUNK_SIZE 100
//     uint16_t buffer[CHUNK_SIZE];
//     for (uint16_t i = 0; i < CHUNK_SIZE; i++) {
//         buffer[i] = blue;
//     }
//
//     // Set DC to data mode and pull CS low for the burst transfer.
//     ST7796_DC_DATA();
//     ST7796_CS_LOW();
//
//         // uint16_t count = (total_pixels > CHUNK_SIZE) ? CHUNK_SIZE : total_pixels;
//         // Transmit the chunk (each pixel is 2 bytes).
//     HAL_GPIO_WritePin(IMD_LED_GPIO_Port, IMD_LED_Pin, GPIO_PIN_RESET);
//     HAL_GPIO_WritePin(BMS_LED_GPIO_Port, BMS_LED_Pin, GPIO_PIN_RESET);
//     HAL_SPI_Transmit(&hspi2, (uint8_t*)buffer, 100, HAL_MAX_DELAY);
//     HAL_GPIO_WritePin(IMD_LED_GPIO_Port, IMD_LED_Pin, GPIO_PIN_SET);
//     HAL_GPIO_WritePin(BMS_LED_GPIO_Port, BMS_LED_Pin, GPIO_PIN_SET);
//         // total_pixels -= count;
//     // ST7796_CS_HIGH();
// }
//
// // --------------------- Example Main Function ---------------------
// // This main function is for illustration. In your project, call ST7796_Init()
// // and then Draw_Blue_Square() as needed (after your HAL and SPI are initialized).
//
// /*
// int main(void) {
//     // HAL initialization
//     HAL_Init();
//     SystemClock_Config();  // User-defined system clock configuration
//     MX_GPIO_Init();        // Initialize GPIOs (including ST7796_CS, ST7796_DC, ST7796_RST)
//     MX_SPI1_Init();        // Initialize SPI1
//
//     // Initialize the display
//     ST7796_Init();
//
//     // Draw the blue square
//     Draw_Blue_Square();
//
//     // Main loop
//     while (1) {
//         // Your application code
//     }
// }
// */
//
