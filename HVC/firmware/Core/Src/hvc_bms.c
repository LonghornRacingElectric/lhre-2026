/**
 * @file hvc_bms.c
 * @brief High Voltage Controller - Battery Management System Implementation
 * 
 * Discharge test: Enable discharge on cell 0 and monitor voltage drop
 */

#include "hvc_bms.h"
#include "hvc_thermistors.h"
#include "can.h"
#include "main.h"
#include "cmsis_os2.h"
#include "adBms6830Data.h"
#include "adBms6830GenericType.h"
#include "adBms6830ParseCreate.h"
#include "adBms_Application.h"
#include "usbd_cdc_if.h"
#include <math.h>
#include <string.h>
#include <stdint.h>

#include "longhorn/rtos/logger.h"

extern SPI_HandleTypeDef hspi4;

// Declare PWM commands locally to avoid multiple definition errors
extern uint8_t WRPWM1[2];
extern uint8_t WRPWM2[2];

// #define TOTAL_IC 10  // 10 BMBs (2 per module)
#define TOTAL_IC 10
#define CELLS_PER_IC 13 // Orion BMS
#define DISCHARGE_CELL 0  // Which cell to discharge (0-4)
// Mask covering all cells (bits 0..CELLS_PER_IC-1)
#define ALL_DISCHARGE_MASK ((1U << CELLS_PER_IC) - 1)

// BMS Safety Thresholds
#define CELL_OVERVOLTAGE_THRESHOLD  4.2f   // Volts
#define CELL_UNDERVOLTAGE_THRESHOLD 2.5f   // Volts
#define CELL_OVERTEMP_THRESHOLD     60.0f  // Celsius


static cell_asic IC[TOTAL_IC];
static uint8_t discharge_active = 0;
static uint8_t bms_error_bmb = 0;      // Which BMB has the error
static uint8_t bms_error_cell = 0;     // Which cell/thermistor has the error
static uint8_t bms_responsive_ics = 0;

// Cell temperature buffer for CAN transmission
static float cell_temps[90];
static float cell_voltages[130];


void bms_init(void)
{
    // Initialize driver structures
    adBms6830_init_config(TOTAL_IC, IC);
}

float bms_get_pack_voltage(void)
{
    // Returns pack voltage in millivolts by summing all cell voltages
    float pack_v = 0;
    uint16_t code;
    int i;
    for (int i = 0; i < TOTAL_IC; i++) {
        for (int j = 0; j < CELLS_PER_IC; j++) {
            code = IC[i].cell.c_codes[j];
            pack_v += (code * 0.000150f) + 1.5f;
        }
    }
    return pack_v;
}

uint8_t get_balance_status(void)
{
    // Return BMS status: 1 if discharging active, 0 otherwise
    return discharge_active ? 1 : 0;
}

uint32_t bms_get_num_responsive_ics(void) {
    return bms_responsive_ics;
}

bool bms_check_undervoltage(void)
{
    bool any_fail = false;

    for (int i = 0; i < TOTAL_IC; i++) {
        for (int j = 0; j < CELLS_PER_IC; j++) {
            uint16_t code = IC[i].cell.c_codes[j];
            float voltage_v = (code * 0.000150f) + 1.5f;
            if (voltage_v < CELL_UNDERVOLTAGE_THRESHOLD) {
                any_fail = true;
                bms_error_bmb = i;
                bms_error_cell = j;
                // log_printf(LOG_ERROR, "BMS ERROR: Undervoltage on BMB %d Cell %d: %.4fV (threshold: %.2fV)",
                //           i, j, voltage_v, CELL_UNDERVOLTAGE_THRESHOLD);
            }
        }
    }

    return any_fail;
}
    
bool bms_check_overvoltage(void)
{
    bool any_fail = false;

    for (int i = 0; i < TOTAL_IC; i++) {
        for (int j = 0; j < CELLS_PER_IC; j++) {
            uint16_t code = IC[i].cell.c_codes[j];
            float voltage_v = (code * 0.000150f) + 1.5f;
            if (voltage_v > CELL_OVERVOLTAGE_THRESHOLD) {
                any_fail = true;
                bms_error_bmb = i;
                bms_error_cell = j;
                // log_printf(LOG_ERROR, "BMS ERROR: Overvoltage on BMB %d Cell %d: %.4fV (threshold: %.2fV)",
                //           i, j, voltage_v, CELL_OVERVOLTAGE_THRESHOLD);
            }
        }
    }

    return any_fail;
}

bool bms_check_overtemp(void) {
    bool any_fail = false;

    for (int i = 0; i < TOTAL_IC; i++) {
        for (int j = 1; j < 10; j++) {
            int16_t code = IC[i].aux.a_codes[j];
            float voltage_v = ((code + 10000) * 0.000150f);
            float temp_c = ntc_voltage_to_temp(voltage_v);
            
            // Check overtemperature (only if valid reading)
            if (isnan(temp_c) || temp_c > CELL_OVERTEMP_THRESHOLD) {
                any_fail = true;
                bms_error_bmb = i;
                bms_error_cell = j;
                // log_printf(LOG_ERROR, "BMS ERROR: Overtemperature on BMB %d Thermistor %d: %.1f°C (threshold: %.1f°C)",
                //           i, j, temp_c, CELL_OVERTEMP_THRESHOLD);
            }
        }
    }

    return any_fail;
}

bool bms_check_disconnection(void) {
    return bms_responsive_ics < TOTAL_IC;
}

float bms_get_min_voltage(void) {
    float min_v = cell_voltages[0];
    for (int i = 1; i < TOTAL_IC * CELLS_PER_IC; i++) {
        if (cell_voltages[i] < min_v && cell_voltages[i] > 0.0f) {
            min_v = cell_voltages[i];
        }
    }
    return min_v;
}

float bms_get_max_voltage(void) {
    float max_v = cell_voltages[0];
    for (int i = 1; i < TOTAL_IC * CELLS_PER_IC; i++) {
        if (cell_voltages[i] > max_v) {
            max_v = cell_voltages[i];
        }
    }
    return max_v;
}

float bms_get_min_temp(void) {
    float min_t = cell_temps[0];
    for (int i = 1; i < TOTAL_IC * 9; i++) {
        if (cell_temps[i] < min_t) {
            min_t = cell_temps[i];
        }
    }
    return min_t;
}

float bms_get_max_temp(void) {
    float max_t = cell_temps[0];
    for (int i = 1; i < TOTAL_IC * 9; i++) {
        if (cell_temps[i] > max_t) {
            max_t = cell_temps[i];
        }
    }
    return max_t;
}

/**
 * @brief Immediately disable all cell discharge and reset balancing state.
 *        Only sends SPI commands if balancing was previously active.
 */
static void bms_disable_discharge(void)
{
    for (int ic = 0; ic < TOTAL_IC; ic++) {
        IC[ic].tx_cfgb.dcc = 0;
        for (int cell = 0; cell < CELLS_PER_IC; cell++) {
            bal_cell_active[ic][cell] = 0;
        }
    }
    // Only write to hardware if we were previously active (avoid redundant SPI traffic)
    if (bal_was_active) {
        adBmsWakeupIc(TOTAL_IC);
        adBms6830_write_config(TOTAL_IC, IC);
        bal_was_active = 0;
    }
    discharge_active = 0;
}
   

void bms_read_thermistors(void)
{
    // Read auxiliary voltages (GPIO pins)
    adBms6830_read_aux_voltages(TOTAL_IC, IC);
    
    // Collect and send cell temperatures via CAN
    int temp_idx = 0;

    for (int i = 0; i < TOTAL_IC; i++) {
        for (int j = 1; j < 10; j++) {  // GPIO 2-9 (8 thermistors per BMB)
            int16_t code = IC[i].aux.a_codes[j];
            float voltage_v = ((code + 10000) * 0.000150f);
            float temp_c = ntc_voltage_to_temp(voltage_v);
            cell_temps[temp_idx++] = isnan(temp_c) ? 0.0f : temp_c;
        }
    }

    hvc_set_cell_temperatures(&cell_temps[0]);
}

void bms_read_cell_voltages(void)
{
    // Read cell voltages
    adBms6830_read_fcell_voltages(TOTAL_IC, IC);

    // Collect and send cell voltages via CAN
    int volt_idx = 0;   

    for (int i = 0; i < TOTAL_IC; i++) {
        for (int j = 0; j < CELLS_PER_IC; j++) {
            int16_t code = IC[i].fcell.fc_codes[j];
            float voltage_v = (code * 0.000150f) + 1.5f;
            cell_voltages[volt_idx++] = voltage_v;
        }
    }

    hvc_set_cell_voltages(&cell_voltages[0]);
}
void bms_update(void)
{

    bms_read_cell_voltages();

    // Read thermistor values
    bms_read_thermistors();

    // check for connectivity
    bms_responsive_ics = 0;
    for(int i = 0; i < TOTAL_IC; i++) {
        bms_responsive_ics += (IC[i].cccrc.cell_pec == 0) && (IC[i].cccrc.aux_pec == 0);
    }
}

void StartBmsTask(void *argument)
{
    osDelay(500);
      
    bms_init();
    
    // Continue monitoring
    for (;;) {
        osDelay(250);
        bms_update();
    }
}