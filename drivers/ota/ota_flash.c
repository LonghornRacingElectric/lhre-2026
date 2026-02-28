#include "ota_flash.h"

#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "main.h"
#include "queue.h"
#include <string.h>

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

static uint8_t residual[8];
static uint8_t residual_len = 0;
static uint32_t write_cursor = 0;
static bool update_active = false;

static FLASH_OBProgramInitTypeDef OBInit;

uint32_t ota_flash_get_bank(uint32_t addr) {
  if (READ_BIT(SYSCFG->MEMRMP, SYSCFG_MEMRMP_FB_MODE) == 0) {
    return (addr < (FLASH_BASE + FLASH_BANK_SIZE)) ? FLASH_BANK_1
                                                   : FLASH_BANK_2;
  } else {
    return (addr < (FLASH_BASE + FLASH_BANK_SIZE)) ? FLASH_BANK_2
                                                   : FLASH_BANK_1;
  }
}

uint32_t ota_flash_get_inactive_bank_base(void) {
  uint32_t running_bank =
      ota_flash_get_bank((uint32_t)ota_flash_get_inactive_bank_base);
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

int ota_flash_get_active_bank(void) {
  return (READ_BIT(SYSCFG->MEMRMP, SYSCFG_MEMRMP_FB_MODE) == 0) ? FLASH_BANK_1
                                                                : FLASH_BANK_2;
}

static void ota_ob_swap_bank(void) {
  HAL_FLASH_Unlock();
  HAL_FLASH_OB_Unlock();
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPTVERR);

  OBInit.OptionType = OPTIONBYTE_USER;
  OBInit.USERType = OB_USER_BFB2;
  HAL_FLASHEx_OBGetConfig(&OBInit);

  OBInit.USERConfig = ((OBInit.USERConfig & OB_BFB2_ENABLE) == OB_BFB2_ENABLE)
                          ? OB_BFB2_DISABLE
                          : OB_BFB2_ENABLE;

  if (HAL_FLASHEx_OBProgram(&OBInit) == HAL_OK) {
    HAL_FLASH_OB_Launch();
  }

  HAL_FLASH_OB_Lock();
  HAL_FLASH_Lock();
}

static void flash_writer_task(void *arg) {
  (void)arg;
  flash_pending_t block;

  for (;;) {
    xQueueReceive(flash_queue, &block, portMAX_DELAY);

    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);

    if (!flash_bank_erased) {
      uint32_t inactive_base = ota_flash_get_inactive_bank_base();
      uint32_t inactive_bank = ota_flash_get_bank(inactive_base);

      FLASH_EraseInitTypeDef erase_init;
      uint32_t page_error = 0;
      erase_init.TypeErase = FLASH_TYPEERASE_MASSERASE;
      erase_init.Banks = inactive_bank;

      if (HAL_FLASHEx_Erase(&erase_init, &page_error) == HAL_OK) {
        flash_bank_erased = true;
      } else {
        HAL_FLASH_Lock();
        continue;
      }
    }

    uint8_t *src = block.data;
    uint16_t remaining = block.length;
    bool write_ok = true;

    while (residual_len > 0 && residual_len < 8 && remaining > 0) {
      residual[residual_len++] = *src++;
      remaining--;
      if (residual_len == 8) {
        uint64_t dword;
        memcpy(&dword, residual, 8);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, write_cursor,
                              dword) != HAL_OK) {
          write_ok = false;
        }
        write_cursor += 8;
        residual_len = 0;
        if (!write_ok)
          break;
      }
    }

    while (write_ok && remaining >= 8) {
      uint64_t dword;
      memcpy(&dword, src, 8);
      if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, write_cursor,
                            dword) != HAL_OK) {
        write_ok = false;
        break;
      }
      write_cursor += 8;
      src += 8;
      remaining -= 8;
    }

    if (write_ok && remaining > 0) {
      memcpy(residual, src, remaining);
      residual_len = remaining;
    }

    HAL_FLASH_Lock();

    if (!write_ok) {
      continue;
    }

    blocks_written++;

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

      osDelay(100);
      ota_ob_swap_bank();
    }
  }
}

void ota_flash_init(void) {
  flash_queue =
      xQueueCreateStatic(FLASH_QUEUE_DEPTH, sizeof(flash_pending_t),
                         (uint8_t *)flash_queue_storage, &flash_queue_cb);

  static const osThreadAttr_t flash_task_attr = {
      .name = "ota_flash_writer",
      .priority = osPriorityAboveNormal,
      .stack_size = 1024,
  };
  osThreadNew(flash_writer_task, NULL, &flash_task_attr);
}

void ota_flash_begin(uint16_t num_blocks) {
  if (update_active) {
    return;
  }
  update_active = true;
  flash_bank_erased = false;
  blocks_written = 0;
  total_blocks_expected = num_blocks;
  residual_len = 0;
  write_cursor = ota_flash_get_inactive_bank_base();
}

void ota_flash_write_memory(uint32_t address, uint8_t *data, uint16_t length) {
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
