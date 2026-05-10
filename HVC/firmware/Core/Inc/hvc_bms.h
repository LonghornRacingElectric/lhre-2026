/**
 * @file hvc_bms.h
 * @brief High Voltage Controller - Battery Management System Interface
 */

#ifndef HVC_BMS_H
#define HVC_BMS_H

#include <stdint.h>
#include <stdbool.h>

void bms_init(void);

void bms_update(void);

void bms_balance_cells(void);
void bms_balance_quick_disable(void);

void bms_read_thermistors(void);
void bms_read_cell_voltages(void);

bool bms_check_disconnection(void);
bool bms_check_undervoltage(void);
bool bms_check_overvoltage(void);
bool bms_check_overtemp(void);

uint8_t bms_get_num_responsive_ics(void);
uint8_t bms_get_balance_status(void);
float bms_get_pack_voltage(void);
float bms_get_min_voltage(void);
float bms_get_max_voltage(void);
float bms_get_min_temp(void);
float bms_get_max_temp(void);

void StartBmsTask(void *argument);

#endif // HVC_BMS_H
