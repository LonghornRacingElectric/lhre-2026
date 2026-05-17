/**
 * @file hvc_bms.c
 * @brief High Voltage Controller - Battery Management System Implementation
 */

#include "hvc_bms.h"
#include "hvc_thermistors.h"
#include "hvc_vct_sense.h"
#include "hvc_state_machine.h"
#include "hvc_states.h"
#include "can.h"
#include "main.h"
#include "cmsis_os2.h"
#include "adBms6830Data.h"
#include "adBms6830GenericType.h"
#include "adBms6830ParseCreate.h"
#include "adBms_Application.h"
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <float.h>
#include "longhorn/rtos/logger.h"

// Pack Geometry
#define TOTAL_IC           10 // 10 ICs, 2 per module
#define CELLS_PER_IC       13 // Orion BMS
#define THERMISTORS_PER_IC 9  // aux GPIO[1..9]; GPIO[0] is reference
#define NUM_CELLS          (TOTAL_IC * CELLS_PER_IC)

// ADBMS6830 voltage code conversion: V = code * 150 uV + 1.5 V
#define ADBMS_LSB_V    0.000150f
#define ADBMS_OFFSET_V 1.5f

// BMS Safety Thresholds
#define CELL_OVERVOLTAGE_THRESHOLD   4.2f  // Volts
#define CELL_UNDERVOLTAGE_THRESHOLD  3.0f  // Volts
#define CELL_OVERTEMP_THRESHOLD      60.0f // Celsius

// BMS Balance Thresholds
#define MAX_BAL_CELLS_PER_BOARD 6
#define BAL_START_MV      15
#define BAL_STOP_MV       7
#define BAL_MIN_CELL_MV   3900
#define PACK_I_MAX_MA     5000
#define BAL_TEMP_CUTOFF_C 60.0f

#define BAL_PERIOD_CYCLES        8
#define THERMISTOR_PERIOD_CYCLES 4

// IC die-temperature derating for internal discharge switches.
// ITMP in Status A: T(C) = (code * 150uV + 1.5V) / 7.5mV/C - 273  (datasheet Table 105)
#define ADBMS_ITMP_LSB_V         0.000150f
#define ADBMS_ITMP_OFFSET_V      1.5f
#define ADBMS_ITMP_SCALE_V_PER_C 0.0075f
#define BAL_DIE_TEMP_REDUCE_C    80.0f  // cap drops to BAL_DIE_TEMP_REDUCE_MAX above here
#define BAL_DIE_TEMP_STOP_C      90.0f  // balancing disabled for that IC above here
#define BAL_DIE_TEMP_REDUCE_MAX  2

static cell_asic IC[TOTAL_IC];
static uint8_t discharge_active = 0;
static uint8_t bms_responsive_ics = 0;

static float cell_temps[TOTAL_IC * THERMISTORS_PER_IC];
static float cell_voltages[NUM_CELLS];

// Persistent balancing state. bal_cmd[] survives across cycles to give the
// hysteresis band a "previous value" to fall back on. bal_dcc_prev[] is the
// last bitmask we pushed to each chip — used to skip redundant SPI writes.
static bool     bal_cmd[NUM_CELLS]       = { false };
static uint16_t bal_dcc_prev[TOTAL_IC]   = { 0 };
static bool     bal_was_active           = false;
static float    ic_die_temps[TOTAL_IC]   = { 0.0f };


void bms_init(void)
{
    // Initialize driver structures
    adBms6830_init_config(TOTAL_IC, IC);

    // Hardware safety net for stuck balancing: will auto-clear its DCC bits after this timeout.
    for (uint8_t ic = 0; ic < TOTAL_IC; ic++) {
        IC[ic].tx_cfgb.dtmen = DTMEN_ON;
    }
    SetConfigB_DischargeTimeOutValue(TOTAL_IC, IC, RANG_0_TO_63_MIN, TIME_1MIN_OR_0_26HR);
    adBmsWakeupIc(TOTAL_IC);
    adBms6830_write_config(TOTAL_IC, IC);
}

float bms_get_pack_voltage(void)
{
    float pack_v = 0.0f;
    for (int i = 0; i < NUM_CELLS; i++) {
        pack_v += cell_voltages[i];
    }
    return pack_v;
}

uint8_t bms_get_balance_status(void)
{
    // Return BMS status: 1 if discharging active, 0 otherwise
    return discharge_active ? 1 : 0;
}

uint8_t bms_get_num_responsive_ics(void) {
    return bms_responsive_ics;
}

// Number of cells currently flagged for bleeding (after the per-board cap).
// Reflects the bal_cmd[] state pushed to the chips on the most recent balance tick.
uint8_t bms_get_balance_count(void) {
    uint8_t n = 0;
    for (int i = 0; i < NUM_CELLS; i++) {
        if (bal_cmd[i]) n++;
    }
    return n;
}

bool bms_check_undervoltage(void)
{
    for (int i = 0; i < NUM_CELLS; i++) {
        if (cell_voltages[i] > 0.0f && cell_voltages[i] < CELL_UNDERVOLTAGE_THRESHOLD) {
            return true;
        }
    }
    return false;
}

bool bms_check_overvoltage(void)
{
    for (int i = 0; i < NUM_CELLS; i++) {
        if (cell_voltages[i] > CELL_OVERVOLTAGE_THRESHOLD) {
            return true;
        }
    }
    return false;
}

bool bms_check_overtemp(void)
{
    // Reads from the cached cell_temps[] array. bms_read_thermistors() masks
    // NaN (open thermistor lead) to 0.0 there, so a single broken sensor
    // doesn't fault the pack — overtemp still trips on any genuine high read.
    for (int i = 0; i < TOTAL_IC * THERMISTORS_PER_IC; i++) {
        if (cell_temps[i] > CELL_OVERTEMP_THRESHOLD) {
            return true;
        }
    }
    return false;
}

// Raw read; debouncing lives in hvc_faults.c (TIMER_BMS_COMMS).
bool bms_check_disconnection(void) {
    return bms_responsive_ics < TOTAL_IC;
}

float bms_get_min_voltage(void) {
    float min_v = FLT_MAX;
    for (int i = 0; i < TOTAL_IC * CELLS_PER_IC; i++) {
        if (cell_voltages[i] < min_v && cell_voltages[i] > 0.0f) {
            min_v = cell_voltages[i];
        }
    }
    return min_v;
}

float bms_get_max_voltage(void) {
    float max_v = FLT_MIN;
    for (int i = 0; i < TOTAL_IC * CELLS_PER_IC; i++) {
        if (cell_voltages[i] > max_v) {
            max_v = cell_voltages[i];
        }
    }
    return max_v;
}

float bms_get_min_temp(void) {
    float min_t = FLT_MAX;
    for (int i = 0; i < TOTAL_IC * THERMISTORS_PER_IC; i++) {
        if (cell_temps[i] < min_t) {
            min_t = cell_temps[i];
        }
    }
    return min_t;
}

float bms_get_max_temp(void) {
    float max_t = FLT_MIN;
    for (int i = 0; i < TOTAL_IC * THERMISTORS_PER_IC; i++) {
        if (cell_temps[i] > max_t) {
            max_t = cell_temps[i];
        }
    }
    return max_t;
}

float bms_get_ic_die_temp(uint8_t ic) {
    if (ic >= TOTAL_IC) return 0.0f;
    return ic_die_temps[ic];
}

// Per-IC balancing cap based on die temperature.
static uint8_t ic_bal_cap(uint8_t ic)
{
    float t = ic_die_temps[ic];
    if (t >= BAL_DIE_TEMP_STOP_C)   return 0;
    if (t >= BAL_DIE_TEMP_REDUCE_C) return BAL_DIE_TEMP_REDUCE_MAX;
    return MAX_BAL_CELLS_PER_BOARD;
}

/*
 * Cap per-board balancing requests using per-IC thermal caps by keeping
 * the cells with the largest delta (cell_v - vmin_v) and clearing the rest.
 * A cap of 0 disables all balancing on that IC.
 */
static void limit_max_cells_per_board(bool balance_cmd[NUM_CELLS],
                                      const float cell_v[NUM_CELLS],
                                      float vmin_v,
                                      const uint8_t caps[TOTAL_IC])
{
    for (uint8_t board = 0; board < TOTAL_IC; board++) {
        const uint16_t base = (uint16_t)board * CELLS_PER_IC;
        const uint8_t  cap  = caps[board];

        uint8_t cell_idx[CELLS_PER_IC];
        float   cell_delta[CELLS_PER_IC];
        uint8_t n_cells = 0;

        for (uint8_t i = 0; i < CELLS_PER_IC; i++) {
            if (balance_cmd[base + i]) {
                float v = cell_v[base + i];
                cell_delta[n_cells] = (v > vmin_v) ? (v - vmin_v) : 0.0f;
                cell_idx[n_cells] = i;
                n_cells++;
            }
        }

        // Already under (or at) the cap — nothing to trim. Also covers n_cells == 0.
        if (n_cells <= cap) {
            continue;
        }

        // Partial selection sort: keep the top cap entries by delta.
        // After this loop, positions [0 .. cap) hold the keepers.
        for (uint8_t a = 0; a < cap; a++) {
            uint8_t best = a;
            for (uint8_t b = a + 1; b < n_cells; b++) {
                if (cell_delta[b] > cell_delta[best]) {
                    best = b;
                }
            }
            if (best != a) {
                float td = cell_delta[a];
                cell_delta[a]    = cell_delta[best];
                cell_delta[best] = td;
                uint8_t ti = cell_idx[a];
                cell_idx[a]    = cell_idx[best];
                cell_idx[best] = ti;
            }
        }

        // Clear every cell that didn't make the top.
        for (uint8_t k = cap; k < n_cells; k++) {
            balance_cmd[base + cell_idx[k]] = false;
        }
    }
}

/**
 * @brief Periodic passive cell-balancing update.
 */
static bool bms_balance_gates_failed(void)
{
    bool driving     = (get_current_state() == HVC_STATE_ENERGIZED);
    int32_t pack_mA  = (int32_t)(get_tractive_current() * 1000.0f);
    bool overcurrent = (pack_mA > PACK_I_MAX_MA) || (pack_mA < -PACK_I_MAX_MA);
    bool fault       = bms_check_disconnection() || bms_check_undervoltage() || bms_check_overtemp();
    bool overtemp    = (bms_get_max_temp() >= BAL_TEMP_CUTOFF_C);
    return fault || driving || overcurrent || overtemp;
}

/*
 * Fast safety path. Called every bms_update() tick (250 ms) on cycles where
 * the full balance decision does NOT run, so a fault between balance ticks
 * still kills the bleeds within one read cycle instead of waiting up to 2 s.
 */
void bms_balance_quick_disable(void)
{
    if (!bal_was_active) return;
    if (!bms_balance_gates_failed()) return;

    for (uint8_t ic = 0; ic < TOTAL_IC; ic++) {
        IC[ic].tx_cfgb.dcc = 0;
        bal_dcc_prev[ic]   = 0;
    }
    adBmsWakeupIc(TOTAL_IC);
    adBms6830_write_config(TOTAL_IC, IC);

    bal_was_active   = false;
    discharge_active = 0;
}

void bms_balance_cells(void)
{
    // Compile-time-folded volt versions of the mV-named tunables.
    const float bal_start_v    = BAL_START_MV    * 0.001f;
    const float bal_stop_v     = BAL_STOP_MV     * 0.001f;
    const float bal_min_cell_v = BAL_MIN_CELL_MV * 0.001f;

    if (bms_balance_gates_failed()) {
        for (int i = 0; i < NUM_CELLS; i++) {
            bal_cmd[i] = false;
        }
    } else {
        // -------- Find vmin over the live cell_voltages[] (volts) --------
        // Skip near-zero reads so a single dropped channel doesn't pin every
        // other cell to a huge delta and fire balancing across the pack.
        float vmin_v = bms_get_min_voltage();

        // -------- Per-cell hysteresis decision --------
        for (int i = 0; i < NUM_CELLS; i++) {
            float v = cell_voltages[i];

            if (v < bal_min_cell_v) {
                bal_cmd[i] = false;          // never bleed a low cell
                continue;
            }
            float delta = v - vmin_v;        // negative deltas just fail both compares

            if (delta > bal_start_v) {
                bal_cmd[i] = true;
            } else if (delta < bal_stop_v) {
                bal_cmd[i] = false;
            }
            // else: in [BAL_STOP_MV, BAL_START_MV] — leave previous value.
        }

        // -------- Per-board cap (thermal derating applied here) --------
        uint8_t caps[TOTAL_IC];
        for (uint8_t ic = 0; ic < TOTAL_IC; ic++) {
            caps[ic] = ic_bal_cap(ic);
        }
        limit_max_cells_per_board(bal_cmd, cell_voltages, vmin_v, caps);
    }

    // -------- Pack into chip dcc bitmasks; write only if changed --------
    bool changed = false;
    bool any_on  = false;
    for (int ic = 0; ic < TOTAL_IC; ic++) {
        uint16_t dcc = 0;
        for (int j = 0; j < CELLS_PER_IC; j++) {
            if (bal_cmd[ic * CELLS_PER_IC + j]) {
                dcc |= (uint16_t)(1u << j);
                any_on = true;
            }
        }
        if (dcc != bal_dcc_prev[ic]) {
            changed = true;
        }
        IC[ic].tx_cfgb.dcc = dcc;
        bal_dcc_prev[ic]   = dcc;
    }

    // Push to hardware on real change, OR when transitioning active->idle
    // (so we guarantee an "all off" frame lands at least once).
    if (changed || (bal_was_active && !any_on)) {
        adBmsWakeupIc(TOTAL_IC);
        adBms6830_write_config(TOTAL_IC, IC);
    }
    bal_was_active   = any_on;
    discharge_active = any_on ? 1 : 0;
}


void bms_read_thermistors(void)
{
    adBms6830_read_aux_voltages(TOTAL_IC, IC);

    int temp_idx = 0;
    for (int i = 0; i < TOTAL_IC; i++) {
        for (int j = 1; j <= THERMISTORS_PER_IC; j++) {  // aux GPIO[1..9]
            int16_t code = IC[i].aux.a_codes[j];
            float voltage_v = code * ADBMS_LSB_V + ADBMS_OFFSET_V;
            float temp_c = ntc_voltage_to_temp(voltage_v);
            cell_temps[temp_idx++] = isnan(temp_c) ? 0.0f : temp_c;
        }
    }

    hvc_set_cell_temperatures(&cell_temps[0]);
}

void bms_read_cell_voltages(void)
{
    adBms6830_read_cell_voltages(TOTAL_IC, IC);

    int volt_idx = 0;
    for (int i = 0; i < TOTAL_IC; i++) {
        for (int j = 0; j < CELLS_PER_IC; j++) {
            int16_t code = IC[i].cell.c_codes[j];
            cell_voltages[volt_idx++] = code * ADBMS_LSB_V + ADBMS_OFFSET_V;
        }
    }

    hvc_set_cell_voltages(&cell_voltages[0]);
}

static void bms_update_connectivity(void)
{
    bms_responsive_ics = 0;
    for (int i = 0; i < TOTAL_IC; i++) {
        bms_responsive_ics += (IC[i].cccrc.cell_pec == 0) && (IC[i].cccrc.aux_pec == 0);
    }
}

static void bms_read_die_temps(void)
{
    uint8_t cmd_rdstata[2] = {0x00, 0x30};
    adBmsWakeupIc(TOTAL_IC);
    adBmsReadData(TOTAL_IC, &IC[0], cmd_rdstata, Status, A);
    for (int i = 0; i < TOTAL_IC; i++) {
        float v = IC[i].stata.itmp * ADBMS_ITMP_LSB_V + ADBMS_ITMP_OFFSET_V;
        ic_die_temps[i] = v / ADBMS_ITMP_SCALE_V_PER_C - 273.0f;
    }
}

void bms_update(void)
{
    static uint32_t bms_tick = 0;
    bms_tick++;

    bool balance_cycle = (bms_tick % BAL_PERIOD_CYCLES) == 0;
    bool temp_cycle = (bms_tick % THERMISTOR_PERIOD_CYCLES) == 0;

    bms_read_cell_voltages();

    if (temp_cycle) {
        bms_read_thermistors();
        bms_read_die_temps();
    }

    bms_update_connectivity();

    if (balance_cycle) {
        bms_balance_cells();
    } else {
        bms_balance_quick_disable();
    }
}

void StartBmsTask(void *argument)
{
    osDelay(500);
      
    bms_init();
    
    for (;;) {
        osDelay(250);
        bms_update();
    }
}