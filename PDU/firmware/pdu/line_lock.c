#include "line_lock.h"

#include "cmsis_os.h"
#include "main.h"
#include "tim.h"

static osThreadAttr_t lineLockTask_attributes = {
    .name = "lineLockTask",
    .priority = (osPriority_t)osPriorityLow,
    .stack_size = 128 * 4,
};

static void line_lock_task(void *argument) {
    (void)argument;

    HAL_TIM_PWM_Start(&htim20, TIM_CHANNEL_3);

    while(1) {

    }
}

void line_lock_init(void) {
    osThreadNew(line_lock_task, NULL, &lineLockTask_attributes);
}
