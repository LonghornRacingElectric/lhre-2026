/**
 * @file lv_port_disp_templ.c
 *
 */

/*Copy this file as "lv_port_disp.c" and set this value to "1" to enable
 * content*/
#include <st7789.h>
#if 1

/*********************
 *      INCLUDES
 *********************/
#include "LCDController.h"
#include "cmsis_os.h"
#include "spi.h"
#include <stdbool.h>

/*********************
 *      DEFINES
 *********************/
#ifndef MY_DISP_HOR_RES
#define MY_DISP_HOR_RES 240
#endif

#ifndef MY_DISP_VER_RES
#define MY_DISP_VER_RES 240
#endif

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void disp_init(void);

static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area,
                       lv_color_t *color_p);

/**********************
 *  STATIC VARIABLES
 **********************/

/* Saved pointer to the display driver — needed in the DMA callback to call
 * lv_disp_flush_ready(). */
static volatile lv_disp_drv_t *flush_disp_drv = NULL;

/* Binary semaphore: given by DMA complete callback, taken by disp_flush
 * to wait for the transfer to finish before telling LVGL it can proceed. */
static osSemaphoreId_t dma_tx_sem;
static const osSemaphoreAttr_t dma_tx_sem_attr = {.name = "dma_tx_sem"};

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_disp_init(void) {
  /*-------------------------
   * Initialize your display
   * -----------------------*/
  disp_init();

  /* Create binary semaphore for DMA synchronization */
  dma_tx_sem = osSemaphoreNew(1, 1, &dma_tx_sem_attr);

  /*-----------------------------
   * Create a buffer for drawing
   *----------------------------*/

  /**
   * LVGL double buffering (option 2):
   * LVGL draws to one buffer while the other is being DMA'd to the display.
   * This makes rendering and flushing parallel.
   */
  static lv_disp_draw_buf_t draw_buf_dsc;
  static lv_color_t buf_1[MY_DISP_HOR_RES * 10]; /*A buffer for 10 rows*/
  static lv_color_t buf_2[MY_DISP_HOR_RES * 10]; /*Second buffer for 10 rows*/
  lv_disp_draw_buf_init(&draw_buf_dsc, buf_1, buf_2,
                        MY_DISP_HOR_RES * 10); /*Initialize with both buffers*/

  /*-----------------------------------
   * Register the display in LVGL
   *----------------------------------*/

  static lv_disp_drv_t disp_drv; /*Descriptor of a display driver*/
  lv_disp_drv_init(&disp_drv);   /*Basic initialization*/

  /*Set the resolution of the display*/
  disp_drv.hor_res = MY_DISP_HOR_RES;
  disp_drv.ver_res = MY_DISP_VER_RES;

  /*Used to copy the buffer's content to the display*/
  disp_drv.flush_cb = disp_flush;

  /*Set the double display buffer*/
  disp_drv.draw_buf = &draw_buf_dsc;

  /*Finally register the driver*/
  lv_disp_drv_register(&disp_drv);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/*Initialize your display and the required peripherals.*/
static void disp_init(void) { ST7789_Init(); }

volatile bool disp_flush_enabled = true;

/* Enable updating the screen (the flushing process) when disp_flush() is called
 * by LVGL
 */
void disp_enable_update(void) { disp_flush_enabled = true; }

/* Disable updating the screen (the flushing process) when disp_flush() is
 * called by LVGL
 */
void disp_disable_update(void) { disp_flush_enabled = false; }

/*Flush the content of the internal buffer the specific area on the display.
 *Uses DMA with semaphore synchronization. */
static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area,
                       lv_color_t *color_p) {
  if (!disp_flush_enabled) {
    lv_disp_flush_ready(disp_drv);
    return;
  }

  /* Wait for any previous DMA transfer to complete before starting a new one.
   * The semaphore is initialized to 1 (available), so the first call goes
   * straight through. Subsequent calls block until the DMA callback gives it
   * back. */
  osSemaphoreAcquire(dma_tx_sem, osWaitForever);

  /* Save the driver pointer so the DMA callback can call flush_ready */
  flush_disp_drv = disp_drv;

  ST7789_SetWindow(area->x1, area->y1, area->x2, area->y2);

  int height = area->y2 - area->y1 + 1;
  int width = area->x2 - area->x1 + 1;

  /* Start non-blocking DMA transfer. */
  ST7789_DrawBitmap_DMA(width, height, (uint8_t *)color_p);
}

/**
 * @brief SPI DMA transfer complete callback (called from ISR).
 *
 * HAL calls this when the DMA transfer started by HAL_SPI_Transmit_DMA
 * finishes. We release CS, tell LVGL the flush is done, and release the
 * semaphore so the next flush can proceed.
 */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
  if (hspi->Instance == SPI2) {
    /* Release chip select */
    HAL_GPIO_WritePin(LCD_NCS_GPIO_Port, LCD_NCS_Pin, GPIO_PIN_SET);

    /* Tell LVGL the flush is complete */
    if (flush_disp_drv != NULL) {
      lv_disp_flush_ready((lv_disp_drv_t *)flush_disp_drv);
    }

    /* Release the semaphore so the next disp_flush can proceed */
    osSemaphoreRelease(dma_tx_sem);
  }
}

#else /*Enable this file at the top*/

/*This dummy typedef exists purely to silence -Wpedantic.*/
typedef int keep_pedantic_happy;
#endif