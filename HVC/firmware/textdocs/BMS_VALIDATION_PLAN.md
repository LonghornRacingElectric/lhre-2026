# HVC BMS Validation Plan

## Hardware Configuration

**BMS Board**: 1x ADBMS6830 monitoring 5 cells  
**Chip Select**: PE3 (verified)  
**SPI Bus**: SPI4  
**Expected Configuration**: 
- 5 active cells (out of 14 possible)
- 5 thermistors (standard per chip)
- 1 chip in daisy chain (not 10)

## Pre-Validation Setup

### 1. Update Firmware for Single-Board Testing

**Modify `NUM_ADBMS_CHIPS` in code:**
- File: `HVC/firmware/App/Inc/adbms6830.h`
- Change: `#define NUM_ADBMS_CHIPS 10` → `#define NUM_ADBMS_CHIPS 1`
- File: `HVC/firmware/App/Inc/hvc_bms.h`
- Change: `#define NUM_BMS_CHIPS 10` → `#define NUM_BMS_CHIPS 1`

**Disable dead cell configuration:**
- File: `HVC/firmware/App/Src/hvc_bms.c`
- Comment out `configure_dead_cells()` and `fix_floating_cells()` calls
- These are for the full 2024 car configuration

**Rebuild:**
```bash
bazel build //HVC/firmware:hvc_firmware_2026_elf --config=debug
```

### 2. Physical Connections

**Power:**
- HVC board powered via USB or bench supply
- BMS board powered appropriately (check ADBMS6830 datasheet - typically 3.3V)

**SPI Connections** (HVC ↔ BMS):
```
HVC PE3  → BMS CS   (Chip Select)
HVC SPI4_MOSI → BMS SDI  (Master Out Slave In)
HVC SPI4_MISO → BMS SDO  (Master In Slave Out)
HVC SPI4_SCK  → BMS SCK  (Clock)
GND           → GND      (Common ground - CRITICAL!)
```

**Cell Connections:**
- Connect 5 cells in series OR
- Use bench power supplies to simulate cells (e.g., 5x 3.7V = 18.5V total)
- Voltage range: 3.0V - 4.2V per cell for valid readings

**Thermistors:**
- If available, connect 5x 10kΩ NTC thermistors
- Otherwise, readings will be invalid but isoSPI communication still testable

### 3. USB CDC Serial Terminal Setup

**Connect to HVC USB port:**
- Device will enumerate as "High Voltage Controller"
- Use serial terminal: PuTTY, RealTerm, or Arduino Serial Monitor
- Baud rate: 115200 (typical, verify with USB CDC config)

## Validation Phases

### Phase 1: isoSPI Communication Validation (15 minutes)

**Objective**: Verify SPI communication with ADBMS6830

**Steps:**
1. Flash firmware with updated chip count
2. Power on HVC board
3. Verify LED rainbow is working (confirms firmware running)
4. Open USB CDC terminal

**Expected Behavior:**
- BMS task runs every 200ms
- `responsive_chips` should equal 1
- `isospi_responsive` should be true

**Debug via GDB or add logging:**
```c
// In StartBmsTask(), add after bms_update():
bms_status_t status;
bms_get_status(&status);
// Log status.responsive_chips via USB CDC
```

**Success Criteria:**
- ✅ No SPI errors
- ✅ Chip responds to wakeup commands
- ✅ CRC validation passes on read commands
- ✅ `responsive_chips == 1` consistently

**Failure Modes:**
- ❌ `responsive_chips == 0`: Check SPI wiring, CS pin, clock polarity/phase
- ❌ Intermittent response: Check ground connection, signal integrity
- ❌ CRC errors: Verify SPI clock speed is appropriate (~1MHz)

---

### Phase 2: Cell Voltage Reading Validation (20 minutes)

**Objective**: Verify cell voltage measurements

**Test Setup:**
- Connect 5 cells or power supplies simulating cells
- Measure actual voltages with multimeter for reference

**Expected Readings:**
- Cell voltages: 3.0V - 4.2V (typical Li-ion range)
- Unused cells (6-14): Should read ~0V or floating values
- Conversion formula: `V = raw_value * 0.00015 + 1.5` (verified in code)

**Validation Steps:**

1. **Read via Debugger:**
   - Set breakpoint in `bms_update()` after cell processing
   - Inspect `cell_voltages[0]` through `cell_voltages[4]`
   - Compare to multimeter readings

2. **Add USB Logging** (recommended):
   ```c
   // In StartBmsTask() after bms_update():
   if (tick_counter % 5 == 0) {  // Every 1 second
       char buf[128];
       sprintf(buf, "Cells: %.3fV %.3fV %.3fV %.3fV %.3fV | Pack: %.2fV\r\n",
               cell_voltages[0], cell_voltages[1], cell_voltages[2],
               cell_voltages[3], cell_voltages[4], pack_voltage);
       // Send via USB CDC
   }
   ```

**Success Criteria:**
- ✅ Readings match multimeter within ±10mV
- ✅ Pack voltage = sum of 5 cell voltages (within ±50mV)
- ✅ Min/max cell voltage tracking works correctly
- ✅ No spurious readings (e.g., negative voltages on live cells)

**Acceptance:**
- Accuracy: ±10mV per cell
- Stability: Readings stable within ±5mV over 10 seconds
- Update rate: New readings every 200ms

---

### Phase 3: Temperature Reading Validation (15 minutes)

**Objective**: Verify thermistor readings (if connected)

**Test Setup:**
- Connect 5x 10kΩ NTC thermistors (β = 3950K typical)
- Thermistors should be at room temperature (~25°C initially)

**Expected Readings:**
- Room temperature: 20-25°C
- LUT range: -40°C to +135°C
- Invalid reading: -999.0°C (out of range or no thermistor)

**Validation Steps:**

1. **Room Temperature Test:**
   - All thermistors should read similar values (within 5°C)
   - Compare to ambient thermometer

2. **Heat Test** (optional):
   - Touch/grip one thermistor
   - Temperature should rise to ~30-35°C
   - Should see change within 5-10 seconds

3. **Add Logging:**
   ```c
   sprintf(buf, "Temps: %.1f %.1f %.1f %.1f %.1f °C\r\n",
           temperatures[0], temperatures[1], temperatures[2],
           temperatures[3], temperatures[4]);
   ```

**Success Criteria:**
- ✅ Room temp readings: 20-30°C range
- ✅ All 5 thermistors give valid readings (not -999.0°C)
- ✅ Min/max temperature tracking works
- ✅ Readings responsive to temperature changes

**Acceptable if thermistors not connected:**
- All readings == -999.0°C
- isoSPI communication still works
- Focus on voltage validation only

---

### Phase 4: Safety Limits Validation (10 minutes)

**Objective**: Verify limit checking and fault detection

**Test Cases:**

**4.1 Undervoltage Test:**
- Lower one cell voltage to 2.9V (below CELL_UNDER_VOLTAGE = 3.0V)
- Expected: `cell_voltages_ok` → false
- State machine should detect fault (via `is_fault_present()`)

**4.2 Overvoltage Test:**
- Raise one cell voltage to 4.3V (above CELL_OVER_VOLTAGE = 4.2V)
- Expected: `cell_voltages_ok` → false

**4.3 Pack Voltage Test:**
- Total pack voltage should be checked
- PACK_UNDER_VOLTAGE = 390V (won't trigger with 5 cells)
- PACK_OVER_VOLTAGE = 546V (won't trigger with 5 cells)
- For testing: Temporarily change limits to ~15V and ~22V

**4.4 Temperature Test** (if thermistors connected):
- Heat one thermistor above 60°C
- Expected: `temperatures_ok` → false

**Success Criteria:**
- ✅ Fault flags set correctly
- ✅ State machine responds to BMS faults
- ✅ No false positives (faults when voltages are normal)

---

### Phase 5: Cell Balancing Validation (20 minutes)

**Objective**: Verify discharge control and balancing logic

**Prerequisites:**
- All 5 cells must be within valid voltage range
- isoSPI communication working
- Balancing enabled: `bms_set_balancing_enable(true)`

**Test Setup:**
- Create cell imbalance: One cell 30-50mV higher than others
- Example: 4 cells at 3.70V, 1 cell at 3.75V

**Expected Behavior:**
```
Min cell: 3.70V
Threshold: 3.70V + 0.030V = 3.730V
Cell at 3.75V should be flagged for discharge
```

**Validation Steps:**

1. **Check Balance Commands:**
   - Inspect `balance_commands[0-4]` array
   - High cell should have `true`, others `false`
   - Balance count should equal number of cells being discharged

2. **Verify ADBMS Configuration:**
   - DCC bits in CFGB register should be set
   - Use oscilloscope on ADBMS discharge pins if accessible

3. **Monitor Balance Progress:**
   - Over 10-30 minutes, high cell voltage should decrease
   - Discharge current: ~200mA typical (check ADBMS datasheet)
   - Expected drop: ~5-10mV per minute

4. **Hysteresis Test:**
   - Once cell drops to 3.72V (min + 0.020V), discharge should stop
   - Verify `balance_commands` updates accordingly

**Success Criteria:**
- ✅ Correct cells identified for balancing
- ✅ DCC bits set in ADBMS configuration
- ✅ Balance count matches expected
- ✅ Hysteresis prevents oscillation
- ✅ Balancing stops when voltage difference < 20mV

---

### Phase 6: Integration with State Machine (10 minutes)

**Objective**: Verify BMS faults trigger state machine response

**Test:**
1. Enter PRECHARGING state (via manual flag or TS_Enable)
2. Create BMS fault (undervoltage one cell)
3. Expected: State machine → NOT_ENERGIZED, contactors open

**Implementation:**
```c
// In hvc_state_machine.c, update is_fault_present():
bool is_fault_present(void) {
    // Check BMS faults
    if (!bms_is_isospi_responsive()) return true;
    if (!bms_are_cell_voltages_ok()) return true;
    if (!bms_are_temperatures_ok()) return true;
    
    // Other fault checks...
    return false;
}
```

**Success Criteria:**
- ✅ BMS faults propagate to state machine
- ✅ State machine takes safe action (open contactors)
- ✅ isoSPI timeout triggers fault
- ✅ System recovers when fault clears

---

## Troubleshooting Guide

### Issue: `responsive_chips == 0`

**Checks:**
1. Verify PE3 is configured as GPIO output for CS
2. Check SPI4 initialization in CubeMX
3. Verify GND connection between boards
4. Measure SPI signals with oscilloscope:
   - CS: Should pulse low during transactions
   - SCK: Should toggle at ~1MHz
   - MOSI: Should show data during writes
5. Check ADBMS6830 power supply (typically 3.3V)

**Common Causes:**
- Missing ground connection
- Incorrect SPI mode (should be Mode 3: CPOL=1, CPHA=1 for ADBMS)
- CS pin not toggling
- BMS board not powered

---

### Issue: CRC Errors (intermittent response)

**Checks:**
1. Reduce SPI clock speed in CubeMX (try 500kHz)
2. Check wire length (keep < 6 inches for prototyping)
3. Add ground plane or twisted pair for SPI signals
4. Check for EMI sources near SPI traces

---

### Issue: Cell Voltage Readings Incorrect

**Checks:**
1. Verify voltage reference in ADBMS is stable
2. Check cell connection polarity
3. Verify conversion formula: `V = raw * 0.00015 + 1.5`
4. Inspect `CFGA` register - REFON bit should be set

**Expected Raw Values:**
- 3.0V → raw ≈ 10000
- 3.7V → raw ≈ 14667  
- 4.2V → raw ≈ 18000

---

### Issue: Temperature Readings Always -999.0°C

**Causes:**
- Thermistors not connected (expected if testing voltage only)
- Vreg not read correctly
- GPIO voltage outside LUT range
- Wrong thermistor value (should be 10kΩ NTC)

**Debug:**
```c
// Check Vreg value (should be ~3.0-3.3V)
// Check GPIO voltage (should be 0.5-2.5V for valid temp range)
```

---

## Success Criteria Summary

### Minimum Viable Validation ✅
- [x] isoSPI communication: 1 chip responding
- [x] Cell voltages: 5 readings accurate to ±10mV
- [x] Pack voltage: Sum of cells within ±50mV
- [x] Safety limits: Faults detected correctly
- [x] No crashes or hard faults in firmware

### Full Validation ✅
- [x] Temperature readings accurate (if thermistors connected)
- [x] Cell balancing: Correct cells identified
- [x] Balance discharge: Measurable voltage change over time
- [x] State machine integration: Faults propagate correctly
- [x] USB CDC logging: Real-time monitoring working

---

## Data Collection for Validation Report

**Record the following:**

1. **Communication Stats:**
   - Responsive chips: X/1
   - CRC error rate: Y errors per 1000 reads
   - isoSPI timeout events: Z occurrences

2. **Voltage Accuracy:**
   ```
   Cell | Multimeter | BMS Reading | Error
   -----|------------|-------------|-------
   1    | 3.712 V    | 3.714 V     | +2mV
   2    | 3.708 V    | 3.710 V     | +2mV
   ...
   ```

3. **Temperature Accuracy** (if applicable):
   ```
   Therm | Thermometer | BMS Reading | Error
   ------|-------------|-------------|-------
   1     | 24.5 °C     | 25.1 °C     | +0.6°C
   ...
   ```

4. **Timing:**
   - BMS update cycle time: ~100ms expected
   - Task period: 200ms (5Hz)

5. **Cell Balancing:**
   - Initial imbalance: ΔV = XmV
   - Time to balance: Y minutes
   - Final imbalance: ΔV = ZmV

---

## Next Steps After Validation

1. **Scale to Full System:**
   - Change `NUM_ADBMS_CHIPS` back to 10
   - Re-enable dead cell configuration
   - Test with full battery pack

2. **Add CAN Communication:**
   - Send cell voltages via CAN to VCU
   - Implement CAN-based BMS status messages

3. **Add USB CDC Logging:**
   - Real-time cell voltage display
   - BMS status dashboard
   - Fault event logging

4. **Optimize Performance:**
   - Profile task execution time
   - Tune SPI clock speed for reliability
   - Implement watchdog for isoSPI timeout

---

**Created**: 2025-11-30  
**Hardware**: 1x ADBMS6830, 5 cells  
**Status**: Ready for validation  
**Estimated Time**: 1.5 - 2 hours total
