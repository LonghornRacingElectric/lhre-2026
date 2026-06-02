#include "steering_switches.h"

#include "longhorn/rtos/logger.h"
#include "main.h"

#include "cmsis_os.h"
#include <cmsis_os2.h>
#include <stdbool.h>

#define STEERING_SWITCH_PRINT_PERIOD_MS 200U

static osThreadAttr_t steering_switch_thread_attrs = {
    .name = "SteerSwitch",
    .priority = osPriorityNormal,
    .stack_size = 128 * 8,
};

static void steering_switch_task(void *argument);
static bool read_pin(GPIO_TypeDef *port, uint16_t pin);

void dui_steering_switches_init(void) {
  // Start printing steering wheel switch states.
  osThreadNew(steering_switch_task, NULL, &steering_switch_thread_attrs);
}

static void steering_switch_task(void *argument) {
  (void)argument;

  while (true) {
    // 1 means the switch is pressed.
    bool top_l_up = read_pin(Switch_Top_L_UP_GPIO_Port, Switch_Top_L_UP_Pin);
    bool top_l_down =
        read_pin(Switch_Top_L_DOWN_GPIO_Port, Switch_Top_L_DOWN_Pin);
    bool top_r_up = read_pin(Switch_Top_R_UP_GPIO_Port, Switch_Top_R_UP_Pin);
    bool top_r_down =
        read_pin(Switch_Top_R_DOWN_GPIO_Port, Switch_Top_R_DOWN_Pin);
    bool bottom_l_up =
        read_pin(Switch_Bottom_L_UP_GPIO_Port, Switch_Bottom_L_UP_Pin);
    bool bottom_l_down =
        read_pin(Switch_Bottom_L_DOWN_GPIO_Port, Switch_Bottom_L_DOWN_Pin);
    bool bottom_r_up =
        read_pin(Switch_Bottom_R_UP_GPIO_Port, Switch_Bottom_R_UP_Pin);
    bool bottom_r_down =
        read_pin(Switch_Bottom_R_DOWN_GPIO_Port, Switch_Bottom_R_DOWN_Pin);

    log_printf(LOG_INFO,
               "SW TL_UP:%u TL_DOWN:%u TR_UP:%u TR_DOWN:%u "
               "BL_UP:%u BL_DOWN:%u BR_UP:%u BR_DOWN:%u\n",
               (unsigned)top_l_up, (unsigned)top_l_down, (unsigned)top_r_up,
               (unsigned)top_r_down, (unsigned)bottom_l_up,
               (unsigned)bottom_l_down, (unsigned)bottom_r_up,
               (unsigned)bottom_r_down);

    osDelay(pdMS_TO_TICKS(STEERING_SWITCH_PRINT_PERIOD_MS));
  }
}

static bool read_pin(GPIO_TypeDef *port, uint16_t pin) {
  return HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_SET;
}
