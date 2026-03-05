#include "cooling.h"

#include "cmsis_os.h"
#include "main.h"
#include "pdu_can.h"

/**
 * Cooling Pump + Fan Declarations
 */

#ifdef COOLING
// Pumps GPIO
#define GPIO_PUMPS_PORT #error "Need to assign GPIO port for pumps"
#define GPIO_PUMPS_PIN #error "Need to assign GPIO pin for pumps"

// Battery Fans 1 PWM
#define PWM_BATTERY_FANS_1_INSTANCE \
    #error "Need to assign timer instance for battery fans 1"
#define PWM_BATTERY_FANS_1_CHANNEL \
    #error "Need to assign timer channel for battery fans 1"
#define GPIO_BATTERY_FANS_1_PORT \
    #error "Need to assign GPIO port for battery fans 1"
#define GPIO_BATTERY_FANS_1_PIN \
    #error "Need to assign GPIO pin for battery fans 1"

// Battery Fans 2 PWM
#define PWM_BATTERY_FANS_2_INSTANCE \
    #error "Need to assign timer instance for battery fans 2"
#define PWM_BATTERY_FANS_2_CHANNEL \
    #error "Need to assign timer channel for battery fans 2"
#define GPIO_BATTERY_FANS_2_PORT \
    #error "Need to assign GPIO port for battery fans 2"
#define GPIO_BATTERY_FANS_2_PIN \
    #error "Need to assign GPIO pin for battery fans 2"

// Radiator Fans PWM
#define PWM_RADIATOR_FANS_INSTANCE \
    #error "Need to assign timer instance for radiator fans PWM"
#define PWM_RADIATOR_FANS_CHANNEL \
    #error "Need to assign timer channel for radiator fans PWM"
#define GPIO_RADIATOR_FANS_PORT \
    #error "Need to assign GPIO port for radiator fans"
#define GPIO_RADIATOR_FANS_PIN \
    #error "Need to assign GPIO pin for radiator fans"
#endif
/**
 * Cooling Task
 */

void cooling_task(void);
osThreadAttr_t coolingTask_attributes = {
    .name = "coolingTask",
    .priority = (osPriority_t)osPriorityNormal,
    .stack_size = 128 * 8};

/**
 * @brief Initializes the cooling system and starts the RTOS task for
 * cooling
 */
void cooling_init(void) {
    // start cooling task
    osThreadNew(cooling_task, NULL, &coolingTask_attributes);
}

void cooling_task(void) {
    while (1) {
        osDelay(100);
    }
}