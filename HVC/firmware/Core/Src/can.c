#include "can.h"

#include <cmsis_os2.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "fdcan.h"
#include "longhorn/can/can_ids.h"
#include "longhorn/rtos/can.h"
#include "longhorn/rtos/logger.h"
#include "task.h"
#include "tim.h"

/* USER CODE BEGIN 0 */

static msg_contactor_status_t contactor_status_tx;

// static msg_vcu_current_sense_t vcu_current_tx;

static msg_indicators_shutdown_status_t indicator_status_tx = {0};

can_interface_t critical_can_bus = {
    .handle = &hfdcan1,
};

void hvc_can_init(void) {

  can_config_t can_config = {
      .init_fn = (CAN_Init_fn)HAL_FDCAN_Init,
      .start_fn = (CAN_Start_fn)HAL_FDCAN_Start,
      .noti_fn = (CAN_ActivateNotifications_fn)HAL_FDCAN_ActivateNotification,
      .stop_fn = (CAN_Stop_fn)HAL_FDCAN_Stop,
      .add_to_queue_fn = (CAN_AddToQ_fn)HAL_FDCAN_AddMessageToTxFifoQ,
      .get_tx_fifo_free_level_fn =
          (CAN_GetTxFifoFreeLevel_fn)HAL_FDCAN_GetTxFifoFreeLevel,
      .get_rx_message_fn = (CAN_GetRxMessage_fn)HAL_FDCAN_GetRxMessage,
      .get_rx_fifo_fill_level_fn =
          (CAN_GetRxFifoFillLevel_fn)HAL_FDCAN_GetRxFifoFillLevel,
      .tick_fn = (Tick_fn)osKernelGetTickCount,
      .add_filter_fn = (CAN_AddFilter_fn)HAL_FDCAN_ConfigFilter,
      .malloc_fn = (Malloc_fn)pvPortMalloc,
      .free_fn = (Free_fn)vPortFree,
      .init_bit = FDCAN_CCCR_INIT,
  };

  critical_can_bus.cccr_reg = &hfdcan1.Instance->CCCR;

  can_rtos_init(&can_config);
  can_rtos_register_interface(&critical_can_bus);

  // static msg_vcu_current_sense_t msg_content;
  // can_message_t* msg = can_get_message_handle(
  //     &msg_content, VCU_CURRENT_SENSE_ID, VCU_CURRENT_SENSE_FREQ,
  //     VCU_CURRENT_SENSE_DLC, pack_vcu_current_sense);
  // can_register_send_packet(&critical_can_bus, msg);

  // TX: contactor status
  can_message_t *contactor_status_handle = can_get_message_handle(
      &contactor_status_tx, CONTACTOR_STATUS_ID, CONTACTOR_STATUS_FREQ,
      CONTACTOR_STATUS_DLC, (CAN_pack_message_fn)pack_contactor_status);
  can_rtos_register_send_packet(&critical_can_bus, contactor_status_handle);

  // TX: indicators + shutdown status
  can_message_t *indicator_status_handle = can_get_message_handle(
      &indicator_status_tx, INDICATORS_SHUTDOWN_STATUS_ID, 10,
      INDICATORS_SHUTDOWN_STATUS_DLC,
      (CAN_pack_message_fn)pack_indicators_shutdown_status);
  can_rtos_register_send_packet(&critical_can_bus, indicator_status_handle);

  can_rtos_start_interface(&critical_can_bus);

  // Start tasks LAST
  can_rtos_start_transceiver_task(osPriorityHigh);
  can_rtos_start_receiver_task(osPriorityHigh);

  log_printf(LOG_INFO, "[HVC] CAN RTOS initialized\n");
}

void hvc_set_contactor_status(int state, bool pos, bool neg) {
  taskENTER_CRITICAL();

  contactor_status_tx.hvc_state_machine = (uint8_t)state;
  contactor_status_tx.positive_hv_contactor = pos ? 1 : 0;
  contactor_status_tx.negative_hv_contactor = neg ? 1 : 0;

  taskEXIT_CRITICAL();
}

void hvc_set_indicator_status(bool bms_error, bool imd_error,
                              bool shutdown_leg1, bool shutdown_leg2,
                              bool shutdown_leg3, bool shutdown_leg4) {
  taskENTER_CRITICAL();

  indicator_status_tx.bms_error = bms_error ? 1 : 0;
  indicator_status_tx.imd_error = imd_error ? 1 : 0;
  indicator_status_tx.shutdown_leg_1 = shutdown_leg1 ? 1 : 0;
  indicator_status_tx.shutdown_leg_2 = shutdown_leg2 ? 1 : 0;
  indicator_status_tx.shutdown_leg_3 = shutdown_leg3 ? 1 : 0;
  indicator_status_tx.shutdown_leg_4 = shutdown_leg4 ? 1 : 0;

  taskEXIT_CRITICAL();
}

/* USER CODE END 0 */