# BMS Validation Quick Start

## Hardware Setup (5 Minutes)

### Connections Required

```
HVC Board (STM32G474)          BMS Board (ADBMS6830)
┌─────────────────────┐        ┌──────────────────────┐
│                     │        │                      │
│  PE3  ─────────────────────> │ CS    (Chip Select)  │
│  SPI4_MOSI (PE14) ──────────> │ SDI   (Data In)      │
│  SPI4_MISO (PE13) <────────── │ SDO   (Data Out)     │
│  SPI4_SCK  (PE12) ──────────> │ SCK   (Clock)        │
│  GND  ─────────────────────> │ GND   (Ground)       │
│                     │        │                      │
│  USB  <───── PC     │        │ Cell1+ <──── 3.7V    │
│                     │        │ Cell1- ───┐          │
└─────────────────────┘        │ Cell2+ <──┤ 3.7V    │
                               │ Cell2- ───┤          │
                               │ Cell3+ <──┤ 3.7V    │
                               │ Cell3- ───┤          │
                               │ Cell4+ <──┤ 3.7V    │
                               │ Cell4- ───┤          │
                               │ Cell5+ <──┘ 3.7V    │
                               │ Cell5- <──── GND     │
                               └──────────────────────┘
```

### Power Supply Options

**Option 1: Real Cells (Recommended)**
- 5x Li-ion cells in series (18.5V nominal)
- Voltage: 3.0V - 4.2V per cell
- **CAUTION**: Ensure proper polarity!

**Option 2: Bench Supplies (Safer for initial test)**
- 5x adjustable power supplies OR
- 1x supply with voltage divider network
- Set each to 3.7V initially

### Tools Needed
- [ ] Multimeter (for voltage verification)
- [ ] USB cable (HVC to PC)
- [ ] Jumper wires (at least 6)
- [ ] Serial terminal software (PuTTY, RealTerm, etc.)

---

## Flash & Test (10 Minutes)

### 1. Flash Firmware
```bash
cd C:\LHRe\LHRe-Monorepo\lhre-2026

# Option A: DFU (press button PB7 during power-up)
bazel run //HVC/firmware:dfu

# Option B: OpenOCD (with ST-Link)
bazel run //HVC/firmware:openocd
```

### 2. Verify Basic Operation
**Power on HVC board**
- ✅ LED should show rainbow pattern
- ✅ Board should enumerate as USB device "High Voltage Controller"

### 3. Connect BMS Board
**Wire SPI connections** (see diagram above)
- **CRITICAL**: Connect GND first!
- Verify with multimeter: continuity between grounds

### 4. Apply Cell Voltages
**Measure each cell with multimeter before connecting**
- Cell 1: _____ V
- Cell 2: _____ V
- Cell 3: _____ V
- Cell 4: _____ V
- Cell 5: _____ V

---

## Test Sequence (30 Minutes)

### Test 1: Communication (5 min)

**Open serial terminal:**
- Baud: 115200
- Port: Check Device Manager for "High Voltage Controller"

**Expected in debugger:**
- `responsive_chips = 1` (check variable in BMS task)
- `isospi_responsive = true`

**If responsive_chips = 0:**
1. Check PE3 toggling with oscilloscope/logic analyzer
2. Verify SPI4 clock on PE12
3. Check ground connection
4. Verify BMS board powered

---

### Test 2: Voltage Reading (10 min)

**Add logging to firmware** (optional but recommended):

In `HVC/firmware/App/Src/hvc_bms.c`, add to `bms_update()` after line completing sequence:
```c
// At end of cmd_sequence == 10 block, add:
static uint32_t log_counter = 0;
if (++log_counter % 5 == 0) {  // Log every 1 second (5 cycles at 200ms)
    // Log via USB CDC or printf
    // TODO: Implement USB CDC logging
}
```

**Verify readings:**
- Compare BMS readings to multimeter (±10mV acceptable)
- Check pack voltage = sum of cells
- Verify min/max tracking

**Record measurements:**
```
Cell | Multimeter | BMS    | Error
-----|------------|--------|-------
1    | _____ V    | _____ V| _____ mV
2    | _____ V    | _____ V| _____ mV
3    | _____ V    | _____ V| _____ mV
4    | _____ V    | _____ V| _____ mV
5    | _____ V    | _____ V| _____ mV
Pack | _____ V    | _____ V| _____ mV
```

---

### Test 3: Fault Detection (10 min)

**Test undervoltage:**
1. Lower Cell 1 to 2.9V (below 3.0V limit)
2. Verify `cell_voltages_ok = false`
3. State machine should detect fault

**Test overvoltage:**
1. Raise Cell 1 to 4.3V (above 4.2V limit)
2. Verify `cell_voltages_ok = false`

**Test recovery:**
1. Return Cell 1 to 3.7V
2. Verify `cell_voltages_ok = true`

---

### Test 4: Cell Balancing (15 min)

**Create imbalance:**
- Set Cell 1: 3.70V
- Set Cell 2: 3.70V  
- Set Cell 3: 3.70V
- Set Cell 4: 3.70V
- Set Cell 5: 3.75V (50mV higher)

**Enable balancing:**
```c
// In state machine or add function call
bms_set_balancing_enable(true);
```

**Verify:**
- Cell 5 should be flagged for discharge
- `balance_count = 1`
- Over 10-30 min, Cell 5 voltage should drop

**Success:**
- ✅ Correct cell identified (Cell 5)
- ✅ Voltage decreases over time
- ✅ Discharge stops when < 3.72V (min + 20mV)

---

## Success Checklist

**Minimum for "BMS Communication Validated":**
- [x] Responsive chips = 1
- [x] Cell voltages read correctly (5 cells)
- [x] Pack voltage = sum of cells
- [x] No SPI errors or timeouts

**Full Validation:**
- [ ] Temperature readings (if thermistors connected)
- [ ] Undervoltage fault detection works
- [ ] Overvoltage fault detection works
- [ ] Cell balancing identifies correct cells
- [ ] State machine responds to BMS faults

---

## Common Issues & Solutions

| Issue | Cause | Solution |
|-------|-------|----------|
| responsive_chips = 0 | SPI not working | Check wiring, CS pin, ground |
| CRC errors | Signal integrity | Reduce SPI clock, shorten wires |
| Wrong voltages | Conversion error | Check REFON bit, verify formula |
| Temp = -999°C | No thermistors | Expected if not connected |
| LED not working | Firmware crash | Check stack sizes, hard fault |

---

## Debug Commands

**View variables in GDB:**
```gdb
break bms_update
continue
print responsive_chips
print cell_voltages[0]@5
print pack_voltage
print min_cell_voltage
```

**Check SPI registers:**
```gdb
print/x hspi4.Instance->SR
print/x hspi4.Instance->DR
```

---

## Next Steps

**After successful validation:**

1. Re-enable full system configuration:
   - `NUM_ADBMS_CHIPS = 10`
   - Uncomment `configure_dead_cells()`
   - Uncomment `fix_floating_cells()`

2. Add USB CDC logging for real-time monitoring

3. Integrate BMS status with CAN communication

4. Test with full 140-cell battery pack

---

**Validation Date**: __________  
**Performed By**: __________  
**Result**: PASS / FAIL  
**Notes**: 
_______________________________________________
_______________________________________________
_______________________________________________
