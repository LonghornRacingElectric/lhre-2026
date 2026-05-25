#include "r2d.h"

#include "dui_can.h"
#include "gpio.h"

#include "cmsis_os.h"
#include "longhorn/rtos/logger.h"
#include "main.h"
#include <cmsis_os2.h>

static osThreadAttr_t r2d_thread_attrs = {
    .name = "R2D",
    .priority = osPriorityNormal,
    .stack_size = 128 * 8,
};

void r2d_task(void *argument);

void dui_r2d_init() { osThreadNew(r2d_task, NULL, &r2d_thread_attrs); }

void r2d_task(void *argument) {

  while (true) {
    bool r2d_enabled = is_r2d_enabled();
    dui_set_r2d(r2d_enabled);
    osDelay(pdMS_TO_TICKS(10));

    dui_can_debug_state_t can_debug = dui_can_get_debug_state();
    log_printf(LOG_INFO,
               "R2D: %u vcu_prndl=%u vcu_r2d_buzzer=%u vcu_timeout=%u "
               "buzzer_active=%u\n",
               (unsigned)r2d_enabled, (unsigned)can_debug.prndl_state,
               (unsigned)can_debug.ready_to_drive_buzzer,
               (unsigned)can_debug.vcu_state_timed_out,
               (unsigned)can_debug.buzzer_active);
  }
}

/**
 * @brief Reads from GPIO pin using HAL
 * @retval True if the ready to drive is enabled, false otherwise
 */
bool is_r2d_enabled(void) {
  return HAL_GPIO_ReadPin(RTD_SWITCH_GPIO_Port, RTD_SWITCH_Pin) ==
         GPIO_PIN_SET;
}
