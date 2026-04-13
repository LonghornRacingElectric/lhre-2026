#include "lvbms_fsm.h"
#include "adBms_Application.h"
#include "adBms6830GenericType.h"
#include "lvbms_temperature_current.h"
#include "main.h"       /* for SHDN_GPIO_Port, SHDN_Pin, hadc1/2/3 */
#include "adc.h"
#include "gpio.h"
#include <stddef.h>
#include "serialPrintResult.h"

/* ── Private state ───────────────────────────────────────────────── */
static BmsState_t  bms_state  = BMS_STATE_IDLE;
static uint32_t    bms_faults = BMS_FAULT_NONE;

static float       cell_v[NUM_CELLS];
static float       temperatures[3];
static float       current;

static cell_asic  *ic;          /* pointer to IC array from app_freertos */
static uint8_t     total_ic;

/* ── Private function declarations ───────────────────────────────── */
static void bms_read_measurements(void);
static void bms_check_faults(void);
static void bms_update_balancing(void);
static void bms_stop_balancing(void);
static void bms_open_contactor(void);
static void bms_close_contactor(void);
static void bms_keep_alive(void);

/* ════════════════════════════════════════════════════════════════════
   Public API
   ════════════════════════════════════════════════════════════════════ */

void bms_task_init(cell_asic *ic_array, uint8_t num_ic)
{
    ic       = ic_array;
    total_ic = num_ic;

    adBms6830_read_config(total_ic, ic);
    adBms6830_init_config(total_ic, ic);
    HAL_GPIO_WritePin(SHDN_GPIO_Port, SHDN_Pin, GPIO_PIN_SET); /* contactor on */


    adBmsWakeupIc(total_ic);
    /* Start continuous C-ADC. DCP_OFF means discharge is paused
       automatically during redundancy checks.                      */
    adBms6830_Adcv(RD_OFF, CONTINUOUS, DCP_OFF, RSTF_OFF, OW_OFF_ALL_CH);
    Delay_ms(8); /* let first conversion complete */

    bms_state  = BMS_STATE_IDLE;
    bms_faults = BMS_FAULT_NONE;
}

void bms_fsm_run(void)
{
    bms_keep_alive();
    switch(bms_state)
    {
    /* ── IDLE ──────────────────────────────────────────────────── */
    case BMS_STATE_IDLE:
        bms_close_contactor();
        bms_state = BMS_STATE_MEASURING;
        break;

    /* ── MEASURING ─────────────────────────────────────────────── */
    case BMS_STATE_MEASURING:
        bms_read_measurements();
        bms_check_faults();

        bms_state = (bms_faults != BMS_FAULT_NONE)
                    ? BMS_STATE_FAULT
                    : BMS_STATE_BALANCING;
        break;

    /* ── BALANCING ─────────────────────────────────────────────── */
    case BMS_STATE_BALANCING:
        bms_update_balancing();
        bms_state = BMS_STATE_IDLE;
        break;

    /* ── FAULT ─────────────────────────────────────────────────── */
    case BMS_STATE_FAULT:
        bms_stop_balancing();
        bms_state = BMS_STATE_SHUTDOWN;
        break;

    /* ── SHUTDOWN ──────────────────────────────────────────────── */
    case BMS_STATE_SHUTDOWN:
        bms_open_contactor();
        /* Keep reading so telemetry/logging still works.
           Stay here until power cycle or external reset.           */
        bms_read_measurements();
        if (bms_faults & BMS_FAULT_OC) {
            break;
        }
        bms_check_faults();
        if (bms_faults == BMS_FAULT_NONE) {
            bms_state = BMS_STATE_IDLE; /* allow restart if fault cleared */
        }
        if (bms_faults & BMS_FAULT_OV) {
            bms_update_balancing(); /* try to balance out of OV fault if possible */
        }
        break;

    default:
        bms_state = BMS_STATE_IDLE;
        break;
    }
}

/* ── Getters ─────────────────────────────────────────────────────── */
BmsState_t bms_get_state(void)               { return bms_state;  }
uint32_t   bms_get_faults(void)              { return bms_faults; }
float      bms_get_cell_voltage(uint8_t i)   { return (i < NUM_CELLS) ? cell_v[i] : 0.0f; }
float      bms_get_temperature(uint8_t i)    { return (i < 3) ? temperatures[i] : 0.0f; }
float      bms_get_current(void)             { return current;    }

/* ════════════════════════════════════════════════════════════════════
   Private helpers
   ════════════════════════════════════════════════════════════════════ */

static void bms_read_measurements(void)
{
    /* ── Cell voltages ── */
    adBms6830_read_avgcell_voltages(total_ic, ic);

    for(uint8_t i = 0; i < NUM_CELLS; i++)
    {
        cell_v[i] = (ic[0].acell.ac_codes[i]+10000) * 0.000150;
    }

    /* ── Temperature and current using your existing function ── */
    ReadTempAndCurrent(temperatures, &current, &hadc1, &hadc2, &hadc3);
    //printf("T1:%.2f T2:%.2f T3:%.2f\r\n", temperatures[0], temperatures[1], temperatures[2]);
    //printf("Current: %.5f A\r\n\n", current);
}

static void bms_check_faults(void)
{
    bms_faults = BMS_FAULT_NONE;

    for(uint8_t i = 0; i < NUM_CELLS; i++)
    {
        if(cell_v[i] > OV_LIMIT_V) bms_faults |= BMS_FAULT_OV;
        if(cell_v[i] < UV_LIMIT_V) bms_faults |= BMS_FAULT_UV;
    }

    /* temperatures[0] and [1] are cell-side NTCs,
       temperatures[2] is the third sensor - use whichever is worst  */
    if(temperatures[0] > OT_LIMIT_C) bms_faults |= BMS_FAULT_OT;
    if(temperatures[1] > OT_LIMIT_C) bms_faults |= BMS_FAULT_OT;
    if(temperatures[2] > OT_LIMIT_C) bms_faults |= BMS_FAULT_OT;

    if(current > OC_LIMIT_A) bms_faults |= BMS_FAULT_OC;
}

static void bms_update_balancing(void)
{
    /* Find lowest cell voltage */
    float min_v = cell_v[0];
    for(uint8_t i = 1; i < NUM_CELLS; i++)
        if(cell_v[i] < min_v) min_v = cell_v[i];

    ic[0].tx_cfgb.dcc = 0;

    if (min_v >= 4.0f) {
        for(uint8_t i = 0; i < NUM_CELLS; i++)
        {
            if(cell_v[i] > (min_v + BALANCE_THRESHOLD_V))
            {
                ic[0].tx_cfgb.dcc |= (1 << i); /* set DCC bit for this cell */
            }
        }
    }

    adBms6830_read_config(total_ic, ic);
    adBms6830_write_config(total_ic, ic);
    //printReadConfig(total_ic, ic, Config, B);
}

static void bms_stop_balancing(void)
{
    ic[0].tx_cfgb.dcc = 0;

    adBms6830_write_config(total_ic, ic);
}

static void bms_open_contactor(void)
{
    /* Pull SHDN low to open the contactor / disable the pack output.
       Change GPIO_PIN_RESET/SET depending on your hardware polarity. */
    HAL_GPIO_WritePin(SHDN_GPIO_Port, SHDN_Pin, GPIO_PIN_RESET);
}

static void bms_close_contactor(void)
{
    /* Pull SHDN high to close the contactor / enable the pack output.
       Change GPIO_PIN_RESET/SET depending on your hardware polarity. */
    HAL_GPIO_WritePin(SHDN_GPIO_Port, SHDN_Pin, GPIO_PIN_SET);
}

static void bms_keep_alive(void)
{
    /* Explicitly reset the BMS watchdog timer.
       Must be called at least once every 2000ms or the chip
       will enter sleep and reset all PWM/DCC/config registers */
    adBmsWakeupIc(total_ic);
}