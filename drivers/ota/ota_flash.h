#ifndef OTA_FLASH_H
#define OTA_FLASH_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void ota_flash_init(void);

void ota_flash_begin(uint16_t num_blocks);
void ota_flash_abort(void);

void ota_flash_write_memory(uint32_t address, uint8_t *data, uint16_t length);
uint32_t ota_flash_get_bank(uint32_t addr);
uint32_t ota_flash_get_inactive_bank_base(void);
int ota_flash_get_active_bank(void);

#ifdef __cplusplus
}
#endif

#endif // OTA_FLASH_H
