/**
 * @file hvc_bms.c
 * @brief High Voltage Controller - Battery Management System Implementation
 * 
 * Discharge test: Enable discharge on cell 0 and monitor voltage drop
 */

#include "hvc_bms.h"
#include "hvc_thermistors.h"
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

#define TOTAL_IC 10  // 10 BMBs (2 per module)
#define CELLS_PER_IC 13 // Orion BMS
#define DISCHARGE_CELL 0  // Which cell to discharge (0-4)
// Mask covering all cells (bits 0..CELLS_PER_IC-1)
#define ALL_DISCHARGE_MASK ((1U << CELLS_PER_IC) - 1)

// BMS Safety Thresholds
#define CELL_OVERVOLTAGE_THRESHOLD  4.2f   // Volts
#define CELL_UNDERVOLTAGE_THRESHOLD 2.5f   // Volts
#define CELL_OVERTEMP_THRESHOLD     60.0f  // Celsius

// BMS Error Flags
#define BMS_ERROR_NONE              0x00
#define BMS_ERROR_OVERVOLTAGE       0x01
#define BMS_ERROR_UNDERVOLTAGE      0x02
#define BMS_ERROR_OVERTEMP          0x04

static cell_asic IC[TOTAL_IC];
static uint8_t discharge_active = 0;
static uint8_t bms_error_bmb = 0;      // Which BMB has the error
static uint8_t bms_error_cell = 0;     // Which cell/thermistor has the error
static uint8_t bms_responsive_ics = 0;

// Returns pack voltage in millivolts by summing all cell voltages
float getPackVoltage_v(void)
{
    float pack_v = 0;
    uint16_t code;
    int i;

    for (int i = 0; i < TOTAL_IC; i++) {
        for (int j = 0; j < CELLS_PER_IC; j++) {
            code = IC[i].cell.c_codes[j];
            // pack_mv += (code * 8) / 30;
            pack_v += (code * 0.000150f) + 1.5f;
        }
    }

    return pack_v;
}

// Return BMS status: 1 if discharging active, 0 otherwise
uint8_t getbmsStatus(void)
{
    return discharge_active ? 1 : 0;
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
                log_printf(LOG_ERROR, "BMS ERROR: Undervoltage on BMB %d Cell %d: %.4fV (threshold: %.2fV)",
                          i, j, voltage_v, CELL_UNDERVOLTAGE_THRESHOLD);
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
                log_printf(LOG_ERROR, "BMS ERROR: Overvoltage on BMB %d Cell %d: %.4fV (threshold: %.2fV)",
                          i, j, voltage_v, CELL_OVERVOLTAGE_THRESHOLD);
            }
        }
    }

    return any_fail;
}


bool bms_check_overtemp(void) {
    bool any_fail = false;

    for (int i = 0; i < TOTAL_IC; i++) {
        for (int j = 1; j < 9; j++) {  // GPIO 2-9 (aux channels 1-8)
            int16_t code = IC[i].aux.a_codes[j];
            float voltage_v = ((code + 10000) * 0.000150f);
            float temp_c = ntc_voltage_to_temp(voltage_v);
            
            // Check overtemperature (only if valid reading)
            if (isnan(temp_c) || temp_c > CELL_OVERTEMP_THRESHOLD) {
                any_fail = true;
                bms_error_bmb = i;
                bms_error_cell = j;
                log_printf(LOG_ERROR, "BMS ERROR: Overtemperature on BMB %d Thermistor %d: %.1f°C (threshold: %.1f°C)",
                          i, j, temp_c, CELL_OVERTEMP_THRESHOLD);
            }
        }
    }

    return any_fail;
}

bool bms_check_disconnection(void) {
    return bms_responsive_ics < TOTAL_IC;
}

void bms_enable_discharge()
{
    char msg[128];
    
    // Set discharge bit for specified cell in Config B register
    // Enable discharge on all cells
    IC[0].tx_cfgb.dcc = (uint16_t)ALL_DISCHARGE_MASK;
    
    // Write configuration with discharge enabled
    adBmsWakeupIc(TOTAL_IC);
    adBms6830_write_config(TOTAL_IC, IC);
    
    // snprintf(msg, sizeof(msg), "Discharge enabled on ALL CELLS (DCC=0x%04X)\r\n", 
    //          IC[0].tx_cfgb.dcc);
    // CDC_Transmit_FS((uint8_t*)msg, strlen(msg));

    log_printf(LOG_INFO, "Discharge enabled on ALL CELLS (DCC=0x%04X)", IC[0].tx_cfgb.dcc);

    
    
    discharge_active = 1;
}

void bms_disable_discharge(void)
{
    // Clear all discharge bits
    IC[0].tx_cfgb.dcc = 0;
    
    // Write configuration with discharge disabled
    adBmsWakeupIc(TOTAL_IC);
    adBms6830_write_config(TOTAL_IC, IC);
    
    // CDC_Transmit_FS((uint8_t*)"Discharge disabled\r\n", 20);
    log_printf(LOG_INFO, "Discharge disabled");
    discharge_active = 0;
}

void bms_init(void)
{
    char msg[128];
    
    // CDC_Transmit_FS((uint8_t*)"=== BMS Discharge Test ===\r\n", 29);
    log_printf(LOG_WARNING, "=== BMS Discharge Test ===");
    
    // Initialize driver structures
    adBms6830_init_config(TOTAL_IC, IC);
    
    // Configure discharge timeout: 63 minutes (maximum in 0-63 minute range)
    // This allows long discharge tests
    // SetConfigB_DischargeTimeOutValue(TOTAL_IC, IC, RANG_0_TO_63_MIN, 63);
    
    // Set PWM duty cycle to 100%
    // SetPwmDutyCycle(TOTAL_IC, IC, PWM_100_0_PCT);
    
    // Initialize Config B with discharge disabled
    // IC[0].tx_cfgb.dcc = 0;

    //Enable BMB Lights on all ICs
    for (int i = 0; i < TOTAL_IC; i++) {
        IC[i].tx_cfga.gpo = 0x000;  // Clear all GPIO bits to enable LEDs
    }
    
    // Wake up chip and write initial configuration
    adBmsWakeupIc(TOTAL_IC);
    adBms6830_write_config(TOTAL_IC, IC);
    
    // Write PWM registers to apply duty cycle settings
    // adBmsWakeupIc(TOTAL_IC);
    // adBmsWriteData(TOTAL_IC, IC, WRPWM1, Pwm, A);
    // adBmsWriteData(TOTAL_IC, IC, WRPWM2, Pwm, B);
    Delay_ms(2);
    
    // Read initial configuration
    adBms6830_read_config(TOTAL_IC, IC);
    
    // snprintf(msg, sizeof(msg), "Initial CFGA: %02X %02X %02X %02X %02X %02X\r\n", 
    //          IC[0].configa.rx_data[0], IC[0].configa.rx_data[1], 
    //          IC[0].configa.rx_data[2], IC[0].configa.rx_data[3],
    //          IC[0].configa.rx_data[4], IC[0].configa.rx_data[5]);

    log_printf(LOG_INFO, 
             "Initial CFGA: %02X %02X %02X %02X %02X %02X",
             IC[0].configa.rx_data[0], IC[0].configa.rx_data[1],
             IC[0].configa.rx_data[2], IC[0].configa.rx_data[3],
             IC[0].configa.rx_data[4], IC[0].configa.rx_data[5]);
             
    log_printf(LOG_INFO, "Reading baseline voltages...");
}

void bms_read_thermistors(void)
{
    // Wake up IC and start auxiliary ADC conversion for GPIO 2-9 (thermistors)
    adBmsWakeupIc(TOTAL_IC);
    adBms6830_Adax(RD_OFF, PUP_DOWN, AUX_ALL);  // Read all auxiliary channels
    
    // Wait for conversion to complete
    Delay_ms(5);
    
    // Read auxiliary voltages (GPIO pins)
    adBms6830_read_aux_voltages(TOTAL_IC, IC);
    
    // Print thermistor readings for each BMB
    for (int i = 0; i < TOTAL_IC; i++) {
        char therm_line[256];
        int offset = 0;
        
        log_printf(LOG_INFO, "BMB %d Thermistors:", i);
        
        // GPIO 2-9 correspond to aux channels 1-8 (GPIO1 is aux[0])
        for (int j = 1; j < 9; j++) {  // Skip GPIO1 (j=0), read GPIO2-9 (j=1-8)
            int16_t code = IC[i].aux.a_codes[j];
            float voltage_v = ((code + 10000) * 0.000150f);  // Convert ADC code to voltage (ADBMS6830 format)
            
            // Convert voltage to temperature
            float temp_c = ntc_voltage_to_temp(voltage_v);
            
            if (!isnan(temp_c)) {
                offset += snprintf(therm_line + offset, sizeof(therm_line) - offset,
                                  "T%d: %.1f°C  ", j, temp_c);
            } else {
                offset += snprintf(therm_line + offset, sizeof(therm_line) - offset,
                                  "T%d: INVALID  ", j);
            }
        }
        
        // Print the complete line
        log_printf(LOG_INFO, "%s\n", therm_line);
        osDelay(10);
    }
}

void bms_update(void)
{
    char msg[128];
    uint16_t code;
    float voltage_v;
    
    // Wake and start ADC conversion
    // Use DCP_ON to keep discharge active during measurement
    adBmsWakeupIc(TOTAL_IC);
    adBms6830_Adcv(RD_ON, CONTINUOUS, DCP_ON, RSTF_OFF, OW_OFF_ALL_CH);
    
    // Wait for conversion
    Delay_ms(5);
    
    // Read cell voltages
    adBms6830_read_cell_voltages(TOTAL_IC, IC);
    
    // Print all cell voltages
    for (int i = 0; i < TOTAL_IC; i++) {
        char cell_line[256];
        int offset = 0;
        
        log_printf(LOG_INFO, "BMB %d Cell Voltages:", i);
        
        // Print cells in groups to fit on lines
        for (int j = 0; j < CELLS_PER_IC; j++) {
            code = IC[i].cell.c_codes[j];
            voltage_v = (code * 0.000150f) + 1.5f;
            
            // Add to line buffer
            offset += snprintf(cell_line + offset, sizeof(cell_line) - offset, 
                              "Cell %d: %.4fV  ", j, voltage_v);
        }
        
        // Print the complete line
        if (discharge_active) {
            log_printf(LOG_WARNING, "%s [DISCHARGING]", cell_line);
        } else {
            log_printf(LOG_INFO, "%s\n", cell_line);
        }
        
        osDelay(10);
    }
    log_printf(LOG_INFO, "Pack Voltage: %.3f V\n", getPackVoltage_v());
    
    // Read thermistor values
    bms_read_thermistors();


    // check for connectivity
    for(int i = 0; i < TOTAL_IC; i++) {
        bms_responsive_ics += (IC[i].cccrc.cell_pec != 0) && (IC[i].cccrc.aux_pec != 0);
    }
    // for(int i = 0; i < TOTAL_IC; i++) {
    //     memset(IC[i].sid.sid, 0, 6);
    // }
    // adBms6830_read_device_sid(TOTAL_IC, IC);
}

void StartBmsTask(void *argument)
{
    osDelay(500);
      
    bms_init();
    
    // Continue monitoring
    for (;;) {
        osDelay(1000);
        bms_update();
    }
}