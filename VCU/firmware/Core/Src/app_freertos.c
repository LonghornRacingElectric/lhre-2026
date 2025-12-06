/* USER CODE BEGIN Header */
/**
 *******************************************************************************
 * File Name          : app_freertos.c
 * Description        : Code for freertos applications
 *******************************************************************************
 */
/* USER CODE END Header */

#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "main.h"
#include "task.h"
#include "usb_device.h"

#include "adc.h"
#include "tim.h"
#include "fdcan.h"
#include "usbd_cdc_if.h"

#include "longhorn/rtos/dfu.h"
#include "longhorn/rtos/led.h"
#include "longhorn/rtos/logger.h"
#include "longhorn/rtos/usb.h"
#include "longhorn/usb_base.h"

#include "vcu_model/inc/vcu_inputs.h"
#include "vcu_model/inc/vcu_model.h"
#include "vcu_model/inc/vcu_outputs.h"

#include "inverter_cm200.h"

#include <stdint.h>

// External handles
extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern ADC_HandleTypeDef hadc3;
extern FDCAN_HandleTypeDef hfdcan1;

// DMA buffers
extern volatile uint16_t adc3_dma_buf[2];   // APPS1, APPS2
volatile uint16_t adc2_dma_buf[2];          // BSE

// Thread handles
osThreadId_t defaultTaskHandle;
osThreadId_t defaultTask2Handle;
osThreadId_t ledHandle;
osThreadId_t adcTaskHandle;
osThreadId_t torqueTaskHandle;

// Thread attributes
const osThreadAttr_t defaultTask_attributes = {
    .name = "defaultTask",
    .priority = osPriorityNormal,
    .stack_size = 128 * 4
};

const osThreadAttr_t adcTask_attributes = {
    .name = "ADC_Task",
    .priority = osPriorityNormal,
    .stack_size = 2048
};

const osThreadAttr_t torqueTask_attributes = {
    .name = "TorqueTask",
    .priority = osPriorityAboveNormal,
    .stack_size = 2048
};

// Task prototypes
void StartDefaultTask(void *argument);
void StartDefaultTask2(void *argument);
void StartADCTask(void *argument);
void StartTorqueTask(void *argument);

void MX_FREERTOS_Init(void)
{
    // Logging / USB / DFU / initial task
    defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

    // LED rainbow thread
    rainbow_led_t led = {
        .ccr2 = &TIM2->CCR1,
        .ccr1 = &TIM2->CCR2,
        .ccr3 = &TIM2->CCR3,
        .channel1 = TIM_CHANNEL_1,
        .channel2 = TIM_CHANNEL_2,
        .channel3 = TIM_CHANNEL_3,
        .pwm_start = (HAL_PWM_Start_Fn)HAL_TIM_PWM_Start,
        .timer_handle = &htim2
    };
    led_init(&led);
    ledHandle = led_start_thread();

    // Second default task (placeholder)
    defaultTask2Handle = osThreadNew(StartDefaultTask2, NULL, NULL);

    // ADC + model inputs task
    adcTaskHandle = osThreadNew(StartADCTask, NULL, &adcTask_attributes);

    // Torque + inverter command task
    torqueTaskHandle = osThreadNew(StartTorqueTask, NULL, &torqueTask_attributes);
}

void StartDefaultTask(void *argument)
{
    MX_USB_Device_Init();

    if (init_logging(CDC_Transmit_FS) == -1) {
        osThreadTerminate(ledHandle);
    }

    dfu_config dfu = {
        .delay_fn = (Delay_fn)osDelay,
        .gpiox = GPIOB,
        .pin = GPIO_PIN_7,
        .pin_set_fn = (PinSet_fn)HAL_GPIO_WritePin,
        .reset_fn = (SystemReset_fn)HAL_NVIC_SystemReset
    };
    init_dfu(dfu);
    dfu_start_thread();

    // Initialize inverter CAN (FDCAN1)
    cm200_init(&hfdcan1);

    for (;;) {
        osDelay(pdMS_TO_TICKS(500));
    }
}

void StartDefaultTask2(void *argument)
{
    for (;;) {
        osDelay(pdMS_TO_TICKS(1000));
    }
}

// ADC task: start DMA and log raw values (APPS/BSE + steering on ADC1)
void StartADCTask(void *argument)
{
    if (HAL_ADC_Start_DMA(&hadc3, (uint32_t*)adc3_dma_buf, 2) != HAL_OK)
        Error_Handler();

    if (HAL_ADC_Start_DMA(&hadc2, (uint32_t*)adc2_dma_buf, 2) != HAL_OK)
        Error_Handler();

    uint32_t adc1_val = 0;

    for (;;) {
        // ADC1: steering or other single-ended sensor
        HAL_ADC_Start(&hadc1);
        if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)
            adc1_val = HAL_ADC_GetValue(&hadc1);
        HAL_ADC_Stop(&hadc1);

        // Kick DMA again if needed
        HAL_ADC_Start(&hadc2);
        HAL_ADC_Start(&hadc3);

        uint16_t apps1 = adc3_dma_buf[0];
        uint16_t apps2 = adc3_dma_buf[1];
        uint16_t bse   = adc2_dma_buf[0];

        log_printf(LOG_INFO,
                   "ADC1:%lu  APPS1:%u  APPS2:%u  BSE:%u\r\n",
                   adc1_val, apps1, apps2, bse);

        osDelay(pdMS_TO_TICKS(300));
    }
}

// Torque task: run VCU model and command inverter
void StartTorqueTask(void *argument)
{
    // Give ADC / logging a little time to start
    osDelay(pdMS_TO_TICKS(200));

    vcu_inputs_t in = (vcu_inputs_t){0};
    vcu_outputs_t out = (vcu_outputs_t){0};

    vcu_model_init();

    for (;;) {
        // Fill model inputs from DMA buffers
        in.apps1_raw = adc3_dma_buf[0];
        in.apps2_raw = adc3_dma_buf[1];
        in.bse_raw   = adc2_dma_buf[0];

        // Run model: APPS/BSE plausibility + torque mapping
        vcu_model_step(&in, &out);

        // Send torque command to inverter (CAN 0x0C0)
        cm200_send_torque(out.torque_cmd);

        // Read any inverter feedback frames (0x0B0, 0x0AC)
        cm200_process_rx();

        // Logging of model state
        int p1_i  = (int)(out.apps1_travel * 1000.0f);
        int p2_i  = (int)(out.apps2_travel * 1000.0f);
        int pf_i  = (int)(out.pedal_filtered * 1000.0f);
        int tq_i  = (int)(out.torque_cmd * 100.0f);
        int psi_i = (int)(out.bse_psi);

        log_printf(LOG_INFO,
            "APP1=%u (%d.%03d)  APP2=%u (%d.%03d)  BSE=%u (%d psi)  ped_f=%d.%03d  tq=%d.%02d Nm  impl=%d  brake_act=%d  brake_lat=%d\r\n",
            in.apps1_raw, p1_i / 1000, p1_i % 1000,
            in.apps2_raw, p2_i / 1000, p2_i % 1000,
            in.bse_raw, psi_i,
            pf_i / 1000, pf_i % 1000,
            tq_i / 100, tq_i % 100,
            out.apps_implaus ? 1 : 0,
            out.brake_active ? 1 : 0,
            out.brake_latched ? 1 : 0);

        // ~20 Hz; can drop to 10–20 ms later if Cascadia wants higher update rate
        osDelay(pdMS_TO_TICKS(50));
    }
}
