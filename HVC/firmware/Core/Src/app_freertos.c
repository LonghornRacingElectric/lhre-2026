/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name          : app_freertos.c
 * Description        : Code for freertos applications
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "main.h"
#include "task.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdint.h>

#include "longhorn/rtos/dfu.h"
#include "longhorn/rtos/led.h"
#include "longhorn/rtos/logger.h"
#include "longhorn/rtos/usb.h"
#include "longhorn/usb_base.h"
#include "tim.h"
#include "usbd_cdc_if.h"

/* HVC Application Modules */
#include "FreeRTOS.h"
#include "adc.h"
#include "can.h"
#include "cmsis_os.h"
#include "fdcan.h"
#include "hvc_bms.h"
#include "hvc_contactors.h"
#include "hvc_state_machine.h"
#include "hvc_vct_sense.h"
#include "tim.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
extern SPI_HandleTypeDef hspi4;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* Definitions for stateMachineTask */
osThreadId_t bmsTaskHandle;
const osThreadAttr_t bmsTask_attributes = {
    .name = "bms_Task",
    .priority = (osPriority_t)osPriorityNormal,  // Same as other tasks
    .stack_size = 256 * 8 * 2};
osThreadId_t stateMachineTaskHandle;
const osThreadAttr_t stateMachineTask_attributes = {
    .name = "stateMachine",
    .priority = (osPriority_t)osPriorityHigh,
    .stack_size = 128 * 8 * 2};

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
    .name = "defaultTask",
    .priority = (osPriority_t)osPriorityNormal,
    .stack_size = 2048 * 4};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void StartStateMachineTask(void* argument);
void StartBmsTask(void* argument);
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void* argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
 * @brief  FreeRTOS initialization
 * @param  None
 * @retval None
 */
void MX_FREERTOS_Init(void) {
    /* USER CODE BEGIN Init */

    /* USER CODE END Init */

    /* USER CODE BEGIN RTOS_MUTEX */
    /* add mutexes, ... */
    /* USER CODE END RTOS_MUTEX */

    /* USER CODE BEGIN RTOS_SEMAPHORES */
    /* add semaphores, ... */
    /* USER CODE END RTOS_SEMAPHORES */

    /* USER CODE BEGIN RTOS_TIMERS */
    /* start timers, add new ones, ... */
    /* USER CODE END RTOS_TIMERS */

    /* USER CODE BEGIN RTOS_QUEUES */
    /* add queues, ... */
    /* USER CODE END RTOS_QUEUES */

    /* Create the thread(s) */
    /* creation of defaultTask */
    defaultTaskHandle =
        osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

    /* USER CODE BEGIN RTOS_THREADS */
    /* add threads, ... */

    /* HVC State Machine Task - 10Hz update rate */
    stateMachineTaskHandle =
        osThreadNew(StartStateMachineTask, NULL, &stateMachineTask_attributes);
    bmsTaskHandle = osThreadNew(StartBmsTask, NULL, &bmsTask_attributes);
    /* Rainbow LED for visual feedback */
    rainbow_led_t led = {
        .ccr2 = &TIM2->CCR1,
        .ccr1 = &TIM2->CCR2,
        .ccr3 = &TIM2->CCR3,
        .channel1 = TIM_CHANNEL_1,
        .channel2 = TIM_CHANNEL_2,
        .channel3 = TIM_CHANNEL_3,
        .pwm_start = (HAL_PWM_Start_Fn)HAL_TIM_PWM_Start,
        .timer_handle = &htim2,
    };

    led_init(&led);
    led_start_thread();

    /* USER CODE END RTOS_THREADS */

    /* USER CODE BEGIN RTOS_EVENTS */
    /* add events, ... */

    /* USER CODE END RTOS_EVENTS */
}

void FakeTask(void* argument) {
    while (1) {
        log_printf(LOG_INFO, "Updating Indicator Status");
        hvc_set_indicator_status(0, 0, 0, 0, 0, 0);
        osDelay(100);
        hvc_set_indicator_status(0, 0, 0, 0, 0, 0);
    }
}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
 * @brief  Function implementing the defaultTask thread.
 * @param  argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void* argument) {
    /* init code for USB_Device */
    MX_USB_Device_Init();
    /* USER CODE BEGIN StartDefaultTask */

    // Wait for USB to enumerate
    osDelay(3000);

    // CDC_Transmit_FS((uint8_t*)msg, strlen(msg));

    // usb_init(CDC_Transmit_FS)
    init_logging(CDC_Transmit_FS);

    const char* msg = "\r\n=== HVC Firmware Starting ===\r\n";
    log_printf(LOG_WARNING, msg);

    osDelay(100);

    // Initialize DFU
    dfu_config dfu = {
        .delay_fn = (Delay_fn)osDelay,
        .gpiox = GPIOB,
        .pin = GPIO_PIN_7,
        .pin_set_fn = (PinSet_fn)HAL_GPIO_WritePin,
        .reset_fn = (SystemReset_fn)HAL_NVIC_SystemReset,
    };

    init_dfu(dfu);

    dfu_start_thread();

    osThreadAttr_t fakeTask_attributes = {
        .name = "FakeTask",
        .priority = (osPriority_t)osPriorityLow,
        .stack_size = 128 * 8 * 2,
    };

    osThreadNew(FakeTask, NULL, &fakeTask_attributes);
    // can_init(&can_config);

    msg = "DFU initialized\r\n";
    log_printf(LOG_INFO, msg);

    msg = "System running\r\n";
    log_printf(LOG_INFO, msg);

    // Initialize CAN (contactor status, data, etc.)
    hvc_can_init();

    /* Infinite loop */
    for (;;) {
        // log_printf(LOG_INFO, "Tractive Voltage: %.2f V, Raw: %d\r\n",
        // get_tractive_voltage(), hvc_adc_read_voltage_sense_raw());
        // log_printf(LOG_INFO, "Is Shutdown Closed: %d\r\n",
        // is_shutdown_closed());
        osDelay(200);
    }
    /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/**
 * @brief State Machine Task
 * @details Runs at 10Hz to manage HVC states, precharge, and contactor control
 * @param argument Not used
 */
void StartStateMachineTask(void* argument) {
    // Wait for USB
    osDelay(3500);
    const char* msg = "State Machine initialized\r\n";
    log_printf(LOG_INFO, msg);
    osDelay(200);

    // Initialize state machine and contactors
    state_machine_init();
    contactors_init();

    // Task loop - 10Hz update rate (100ms period)
    const uint32_t task_period_ms = 100;

    for (;;) {
        // Update state machine
        update_state_machine();
        hvc_set_contactor_status(get_current_state(),
                                 get_drive_contactors_state(),
                                 get_drive_contactors_state());
        /*
        log_printf(LOG_INFO, "Tractive Voltage: %.2f V, Raw: %d  "
                             "HVC State: %s  "
                             "Is Shutdown Closed: %d  "
                             "HVC Contactors: %d\r\n",
                             get_tractive_voltage(),
        hvc_adc_read_voltage_sense_raw(), get_state_name(get_current_state()),
                             is_shutdown_closed(),
                             get_drive_contactors_state());
                    */
        osDelay(task_period_ms);
    }
}

/* USER CODE END Application */
