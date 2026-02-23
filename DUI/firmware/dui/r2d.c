#include "r2d.h"

#include "dui_can.h"
#include "gpio.h"

#include "cmsis_os.h"
#include "longhorn/rtos/logger.h"
#include "main.h"
#include <cmsis_os2.h>

static osThreadAttr_t r2d_thread_attrs = {
    .name = "R2D",
    .priority = osPriorityHigh,
    .stack_size = 128 * 8,
};

void r2d_task(void *argument);

void dui_r2d_init() { osThreadNew(r2d_task, NULL, &r2d_thread_attrs); }

void r2d_task(void *argument) {

  while (true) {
    dui_set_r2d(is_r2d_enabled());
    osDelay(pdMS_TO_TICKS(10));
    log_printf(LOG_INFO, "R2D: %d\n", is_r2d_enabled());
  }
}

/**
 * @brief Reads from GPIO pin using HAL
 * @retval True if the ready to drive is enabled, false otherwise
 */
bool is_r2d_enabled(void) {
  return HAL_GPIO_ReadPin(RTD_SWITCH_GPIO_Port, RTD_SWITCH_Pin) ==
         GPIO_PIN_RESET;
}