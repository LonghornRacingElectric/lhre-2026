#include "dui_can.h"

#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "fdcan.h"
#include "longhorn/fw_update.h"
#include "longhorn/rtos/can.h"
#include "longhorn/rtos/logger.h"
#include "queue.h"
#include "semphr.h"
#include "stdint.h"
#include <stm32g474xx.h>
#include <string.h>

/** ==
 *  CAN Interface and Configuration Setup
 *  ==
 */

static can_interface_t critical_bus = {
    .handle = &hfdcan1,
};

/** ==
 * Outgoing CAN Packets
 *  ==
 */

static msg_dui_r2d_status_t r2d_status_mailbox = {0};
static can_message_t *r2d_status_mailbox_handle = NULL;

/** ==
 * Incoming CAN Packets
 *  ==
 */

static msg_indicators_shutdown_status_t indicator_status_mailbox = {0};
static can_receive_message_t *indicator_status_mailbox_handle = NULL;

void dui_can_add_receive_handlers(void);
void dui_can_add_send_handlers(void);

#define FLASH_PENDING_BUF_SIZE 255

#define FLASH_QUEUE_DEPTH 10

typedef struct {
  uint8_t data[FLASH_PENDING_BUF_SIZE];
  uint32_t address;
  uint16_t length;
} flash_pending_t;

static flash_pending_t flash_queue_storage[FLASH_QUEUE_DEPTH];
static StaticQueue_t flash_queue_cb;
static QueueHandle_t flash_queue = NULL;
static bool flash_bank_erased = false;
static uint16_t blocks_written = 0;
static uint16_t total_blocks_expected = 0;

static FLASH_EraseInitTypeDef EraseInitStruct;
static FLASH_OBProgramInitTypeDef OBInit;
static uint32_t PAGEError = 0;

static uint8_t residual[8];
static uint8_t residual_len = 0;
static uint32_t write_cursor = 0;
static bool update_active = false;

static uint32_t GetBank(uint32_t Addr) {
  if (READ_BIT(SYSCFG->MEMRMP, SYSCFG_MEMRMP_FB_MODE) == 0) {
    return (Addr < (FLASH_BASE + FLASH_BANK_SIZE)) ? FLASH_BANK_1
                                                   : FLASH_BANK_2;
  } else {
    return (Addr < (FLASH_BASE + FLASH_BANK_SIZE)) ? FLASH_BANK_2
                                                   : FLASH_BANK_1;
  }
}

static uint32_t get_inactive_bank_base(void) {
  uint32_t running_bank = GetBank((uint32_t)get_inactive_bank_base);
  if (running_bank == FLASH_BANK_1) {
    return (READ_BIT(SYSCFG->MEMRMP, SYSCFG_MEMRMP_FB_MODE) == 0)
               ? (FLASH_BASE + FLASH_BANK_SIZE)
               : FLASH_BASE;
  } else {
    return (READ_BIT(SYSCFG->MEMRMP, SYSCFG_MEMRMP_FB_MODE) == 0)
               ? FLASH_BASE
               : (FLASH_BASE + FLASH_BANK_SIZE);
  }
}

int get_active_bank(void) {
  return (READ_BIT(SYSCFG->MEMRMP, SYSCFG_MEMRMP_FB_MODE) == 0) ? FLASH_BANK_1
                                                                : FLASH_BANK_2;
}

// uses queue off the ISR to write to flash
static void flash_writer_task(void *arg) {
  (void)arg;
  flash_pending_t block;

  for (;;) {
    xQueueReceive(flash_queue, &block, portMAX_DELAY);

    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);

    // erase inactive bank once
    if (!flash_bank_erased) {
      uint32_t inactive_base = get_inactive_bank_base();
      uint32_t inactive_bank = GetBank(inactive_base);

      EraseInitStruct.TypeErase = FLASH_TYPEERASE_MASSERASE;
      EraseInitStruct.Banks = inactive_bank;

      if (HAL_FLASHEx_Erase(&EraseInitStruct, &PAGEError) == HAL_OK) {
        flash_bank_erased = true;
      } else {
        log_printf(LOG_ERROR, "[DUI] Flash erase failed\n");
        HAL_FLASH_Lock();
        continue;
      }
    }

    // start writing, keep a residual so we can write doublewords at 8 byte
    // boundaries
    uint8_t *src = block.data;
    uint16_t remaining = block.length;
    bool write_ok = true;

    // fill residual from previous block to form a complete doubleword
    while (residual_len > 0 && residual_len < 8 && remaining > 0) {
      residual[residual_len++] = *src++;
      remaining--;
      if (residual_len == 8) {
        uint64_t dword;
        memcpy(&dword, residual, 8);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, write_cursor,
                              dword) != HAL_OK) {
          log_printf(LOG_ERROR, "[DUI] Flash program failed at 0x%08lX\n",
                     write_cursor);
          write_ok = false;
        }
        // new address is + 8 bytes
        write_cursor += 8;
        residual_len = 0;
        if (!write_ok)
          break;
      }
    }

    // write the double words to flash
    while (write_ok && remaining >= 8) {
      uint64_t dword;
      memcpy(&dword, src, 8);
      if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, write_cursor,
                            dword) != HAL_OK) {
        log_printf(LOG_ERROR, "[DUI] Flash program failed at 0x%08lX\n",
                   write_cursor);
        write_ok = false;
        break;
      }
      write_cursor += 8;
      src += 8;
      remaining -= 8;
    }

    // keep the extra bytes for next block
    if (write_ok && remaining > 0) {
      memcpy(residual, src, remaining);
      residual_len = remaining;
    }

    HAL_FLASH_Lock();

    if (!write_ok) {
      continue;
    }

    blocks_written++;
    log_printf(LOG_INFO, "[DUI] Flash block %u/%u written\n", blocks_written,
               total_blocks_expected);

    // write the last blocks we had
    if (total_blocks_expected > 0 && blocks_written >= total_blocks_expected) {
      if (residual_len > 0) {
        memset(residual + residual_len, 0xFF, 8 - residual_len);
        uint64_t dword;
        memcpy(&dword, residual, 8);
        HAL_FLASH_Unlock();
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, write_cursor, dword);
        HAL_FLASH_Lock();
        residual_len = 0;
      }

      log_printf(LOG_INFO,
                 "[DUI] All blocks written. Launching new firmware.\n");

      HAL_FLASH_Unlock();
      HAL_FLASH_OB_Unlock();
      __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPTVERR);

      OBInit.OptionType = OPTIONBYTE_USER;
      OBInit.USERType = OB_USER_BFB2;
      HAL_FLASHEx_OBGetConfig(&OBInit);

      OBInit.USERConfig =
          ((OBInit.USERConfig & OB_BFB2_ENABLE) == OB_BFB2_ENABLE)
              ? OB_BFB2_DISABLE
              : OB_BFB2_ENABLE;

      if (HAL_FLASHEx_OBProgram(&OBInit) == HAL_OK) {
        // give it a second to flush logs and to also make sure everything looks
        // good
        osDelay(100);
        HAL_FLASH_OB_Launch();
      }

      HAL_FLASH_OB_Lock();
      HAL_FLASH_Lock();
    }
  }
}

// called when we are starting update, so we can initialize the update state
void dui_fw_update_begin(uint16_t num_blocks) {
  if (update_active) {
    return;
  }
  update_active = true;
  flash_bank_erased = false;
  blocks_written = 0;
  total_blocks_expected = num_blocks;
  residual_len = 0;
  write_cursor = get_inactive_bank_base();
}

// write memory callback so that the lib knows how to write to our codebase
void write_memory(uint32_t address, uint8_t *data, uint16_t length) {
  if (flash_queue == NULL) {
    return;
  }

  flash_pending_t block;
  block.address = address;
  block.length =
      (length <= FLASH_PENDING_BUF_SIZE) ? length : FLASH_PENDING_BUF_SIZE;
  memcpy(block.data, data, block.length);

  BaseType_t higher_prio_woken = pdFALSE;
  xQueueSendFromISR(flash_queue, &block, &higher_prio_woken);
  portYIELD_FROM_ISR(higher_prio_woken);
}

/**
 * @brief Initializes the CAN interface with the RTOS library and registers
 * handlers. Also starts the CAN transceiver and receiver tasks.
 */
void dui_can_init(void) {
  /* Create the block queue before the CAN ISR can fire.
   * Uses static storage so no heap allocation is needed. */
  flash_queue =
      xQueueCreateStatic(FLASH_QUEUE_DEPTH, sizeof(flash_pending_t),
                         (uint8_t *)flash_queue_storage, &flash_queue_cb);

  static const osThreadAttr_t flash_task_attr = {
      .name = "flash_writer",
      .priority = osPriorityAboveNormal,
      .stack_size = 1024,
  };
  osThreadNew(flash_writer_task, NULL, &flash_task_attr);

  can_config_t dui_can_config = {
      .init_fn = (CAN_Init_fn)HAL_FDCAN_Init,
      .start_fn = (CAN_Start_fn)HAL_FDCAN_Start,
      .noti_fn = (CAN_ActivateNotifications_fn)HAL_FDCAN_ActivateNotification,
      .stop_fn = (CAN_Stop_fn)HAL_FDCAN_Stop,
      .add_to_queue_fn = (CAN_AddToQ_fn)HAL_FDCAN_AddMessageToTxFifoQ,
      .get_tx_fifo_free_level_fn =
          (CAN_GetTxFifoFreeLevel_fn)HAL_FDCAN_GetTxFifoFreeLevel,
      .get_rx_message_fn = (CAN_GetRxMessage_fn)HAL_FDCAN_GetRxMessage,
      .tick_fn = HAL_GetTick,
      .add_filter_fn = (CAN_AddFilter_fn)HAL_FDCAN_ConfigFilter,
      .malloc_fn = pvPortMalloc,
      .free_fn = vPortFree,
      .init_bit = FDCAN_CCCR_INIT,
      .device_id = DEVICE_ID_DUI,
      .write_memory_fn = write_memory,
      .fw_update_begin_fn = dui_fw_update_begin,
  };

  can_rtos_init(&dui_can_config);

  critical_bus.cccr_reg = &hfdcan1.Instance->CCCR;

  // Register physical interfaces FIRST
  can_rtos_register_interface(&critical_bus);

  dui_can_add_send_handlers();
  taskENTER_CRITICAL();
  dui_can_add_receive_handlers();
  taskEXIT_CRITICAL();

  can_rtos_start_interface(&critical_bus);

  can_rtos_start_transceiver_task(osPriorityNormal);
  can_rtos_start_receiver_task(osPriorityAboveNormal);

  log_printf(LOG_INFO, "[DUI] CAN RTOS initialized\n");
}

/**
 * @brief Creates the CAN receive handlers and registers them with the CAN lib
 *
 * Forward declaration is above, but this API is NOT public, so don't call in
 * other files
 *
 */
void dui_can_add_receive_handlers(void) {
  // Indicators + Shutdown Status
  indicator_status_mailbox_handle = can_get_receive_message_handle(
      &indicator_status_mailbox, INDICATORS_SHUTDOWN_STATUS_ID,
      (CAN_unpack_message_fn)unpack_indicators_shutdown_status);

  can_rtos_register_receive_packet(&critical_bus,
                                   indicator_status_mailbox_handle);

  log_printf(LOG_INFO, "[DUI] CAN IMD + BMS handlers registered\n");
}

/**
 * @brief Creates the CAN send handlers and registers them with the CAN lib
 *
 * Forward declaration is above, but this API is NOT public, so don't call in
 * other files
 *
 */
void dui_can_add_send_handlers(void) {
  r2d_status_mailbox_handle = can_get_message_handle(
      &r2d_status_mailbox, DUI_R2D_STATUS_ID, DUI_R2D_STATUS_FREQ,
      DUI_R2D_STATUS_DLC, (CAN_pack_message_fn)pack_dui_r2d_status);
  can_rtos_register_send_packet(&critical_bus, r2d_status_mailbox_handle);
}

/**
 * @brief Check if there are faults. Checks the mailbox that is updated by the
 * CAN RTOS tasks. Then, returns true if there is a fault present OR if there
 * was a timeout.
 *
 * @return true
 * @return false
 */
bool hvc_imd_fault(void) {
  return indicator_status_mailbox.imd_error ||
         message_timed_out(indicator_status_mailbox_handle,
                           INDICATORS_SHUTDOWN_STATUS_TIMEOUT_MS * 4);
}

bool hvc_bms_fault(void) {
  return indicator_status_mailbox.bms_error ||
         message_timed_out(indicator_status_mailbox_handle,
                           INDICATORS_SHUTDOWN_STATUS_TIMEOUT_MS * 4);
}

void dui_set_r2d(bool enabled) { r2d_status_mailbox.r2d_status = enabled; }