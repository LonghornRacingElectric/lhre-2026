#ifndef ST7789_H
#define ST7789_H
#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

void ST7789_Init(void);
void ST7789_SetWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void ST7789_WriteData(uint8_t data);
void ST7789_DrawBitmap(uint16_t w, uint16_t h, const uint8_t *bitmap);

#endif /* ST7789_H */