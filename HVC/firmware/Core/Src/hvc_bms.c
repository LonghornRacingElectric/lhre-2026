/**
 * @file hvc_bms.c
 * @brief High Voltage Controller - Battery Management System Implementation
 * 
 * Discharge test: Enable discharge on cell 0 and monitor voltage drop
 */

#include "hvc_bms.h"
#include "main.h"
#include "cmsis_os2.h"
#include "adBms6830Data.h"
#include "adBms6830GenericType.h"
#include "adBms6830ParseCreate.h"
#include "adBms_Application.h"
#include "usbd_cdc_if.h"
#include <string.h>
#include <stdint.h>

extern SPI_HandleTypeDef hspi4;

// Declare PWM commands locally to avoid multiple definition errors
extern uint8_t WRPWM1[2];
extern uint8_t WRPWM2[2];

#define TOTAL_IC 1
#define NUM_CELLS 5
#define DISCHARGE_CELL 0  // Which cell to discharge (0-4)
// Mask covering all cells (bits 0..NUM_CELLS-1)
#define ALL_DISCHARGE_MASK ((1U << NUM_CELLS) - 1)

static cell_asic IC[TOTAL_IC];
static uint8_t discharge_active = 0;

// Returns pack voltage in millivolts by summing all cell voltages
uint32_t getPackVoltage_mv(void)
{
    uint32_t pack_mv = 0;
    uint16_t code;
    int i;

    for (i = 0; i < NUM_CELLS; i++) {
        code = IC[0].cell.c_codes[i];
        pack_mv += (uint32_t)((code * 8) / 30);
    }

    return pack_mv;
}

// Return BMS status: 1 if discharging active, 0 otherwise
uint8_t getbmsStatus(void)
{
    return discharge_active ? 1 : 0;
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
    
    snprintf(msg, sizeof(msg), "Discharge enabled on ALL CELLS (DCC=0x%04X)\r\n", 
             IC[0].tx_cfgb.dcc);
    CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
    
    discharge_active = 1;
}

void bms_disable_discharge(void)
{
    // Clear all discharge bits
    IC[0].tx_cfgb.dcc = 0;
    
    // Write configuration with discharge disabled
    adBmsWakeupIc(TOTAL_IC);
    adBms6830_write_config(TOTAL_IC, IC);
    
    CDC_Transmit_FS((uint8_t*)"Discharge disabled\r\n", 20);
    discharge_active = 0;
}

void bms_init(void)
{
    char msg[128];
    
    CDC_Transmit_FS((uint8_t*)"=== BMS Discharge Test ===\r\n", 29);
    
    // Initialize driver structures
    adBms6830_init_config(TOTAL_IC, IC);
    
    // Configure discharge timeout: 63 minutes (maximum in 0-63 minute range)
    // This allows long discharge tests
    SetConfigB_DischargeTimeOutValue(TOTAL_IC, IC, RANG_0_TO_63_MIN, 63);
    
    // Set PWM duty cycle to 100%
    SetPwmDutyCycle(TOTAL_IC, IC, PWM_100_0_PCT);
    
    // Initialize Config B with discharge disabled
    IC[0].tx_cfgb.dcc = 0;
    
    // Wake up chip and write initial configuration
    adBmsWakeupIc(TOTAL_IC);
    adBms6830_write_config(TOTAL_IC, IC);
    
    // Write PWM registers to apply duty cycle settings
    adBmsWakeupIc(TOTAL_IC);
    adBmsWriteData(TOTAL_IC, IC, WRPWM1, Pwm, A);
    adBmsWriteData(TOTAL_IC, IC, WRPWM2, Pwm, B);
    Delay_ms(2);
    
    // Read initial configuration
    adBms6830_read_config(TOTAL_IC, IC);
    
    snprintf(msg, sizeof(msg), "Initial CFGA: %02X %02X %02X %02X %02X %02X\r\n", 
             IC[0].configa.rx_data[0], IC[0].configa.rx_data[1], 
             IC[0].configa.rx_data[2], IC[0].configa.rx_data[3],
             IC[0].configa.rx_data[4], IC[0].configa.rx_data[5]);
    CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
    
    CDC_Transmit_FS((uint8_t*)"Reading baseline voltages...\r\n", 31);
}

void bms_update(void)
{
    char msg[128];
    uint16_t code;
    uint32_t voltage_mv;
    int i;
    
    // Wake and start ADC conversion
    // Use DCP_ON to keep discharge active during measurement
    adBmsWakeupIc(TOTAL_IC);
    adBms6830_Adcv(RD_ON, CONTINUOUS, DCP_ON, RSTF_OFF, OW_OFF_ALL_CH);
    
    // Wait for conversion
    Delay_ms(5);
    
    // Read cell voltages
    adBms6830_read_cell_voltages(TOTAL_IC, IC);
    
    // Print all cell voltages, marking the discharging cell
    for (i = 0; i < NUM_CELLS; i++) {
        code = IC[0].cell.c_codes[i];
        voltage_mv = (code * 8) / 30;  // Convert to mV
        
        if (discharge_active) {
            snprintf(msg, sizeof(msg), "Cell %d: %lu mV [DISCHARGING]\r\n", i, voltage_mv);
        } else {
            snprintf(msg, sizeof(msg), "Cell %d: %lu mV\r\n", i, voltage_mv);
        }
        CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
    }
}

void StartBmsTask(void *argument)
{
    uint32_t loop_count;
    
    osDelay(3500);  // Wait for USB
    
    CDC_Transmit_FS((uint8_t*)"[BMS] Starting discharge test\r\n", 32);
    
    bms_init();
    
    // Read baseline voltages for 3 seconds
    for (loop_count = 0; loop_count < 3; loop_count++) {
        osDelay(1000);
        bms_update();
    }
    
    // Enable discharge on specified cell
    bms_enable_discharge();

    // Monitor for 500 seconds with discharge active
    CDC_Transmit_FS((uint8_t*)"\r\n*** DISCHARGE ACTIVE FOR 500 SECONDS ***\r\n\r\n", 48);
    for (loop_count = 0; loop_count < 500; loop_count++) {
        osDelay(1000);
        bms_update();
    }
    
    // Disable discharge
    bms_disable_discharge();
    
    // Continue monitoring
    CDC_Transmit_FS((uint8_t*)"\r\n*** DISCHARGE STOPPED - MONITORING ***\r\n\r\n", 44);
    for (;;) {
        osDelay(1000);
        bms_update();
    }
}