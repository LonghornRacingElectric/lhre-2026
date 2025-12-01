# HVC Application Logic

This directory contains the high-level application logic for the High Voltage Controller (HVC) firmware, ported from the 2024 bare-metal implementation to the 2026 FreeRTOS architecture.

## Architecture Overview

The HVC firmware follows a modular, task-based RTOS architecture:

```
┌─────────────────────────────────────────────────────┐
│                  FreeRTOS Kernel                    │
├─────────────────────────────────────────────────────┤
│  State Machine Task (10Hz, High Priority)           │
│  ├─ Precharge Logic                                 │
│  ├─ Contactor Control                               │
│  └─ State Transitions                               │
├─────────────────────────────────────────────────────┤
│  Future Tasks:                                       │
│  ├─ BMS/Cells Task (5Hz)                            │
│  ├─ Measurements Task (10Hz)                        │
│  ├─ CAN Communication Task (100Hz)                  │
│  └─ Safety Monitor Task (100Hz)                     │
└─────────────────────────────────────────────────────┘
```

## Module Structure

### State Machine (`hvc_state_machine.h/c`)

The core state machine manages five states:
1. **NOT_ENERGIZED** - System powered down, all contactors open
2. **PRECHARGING** - Precharge resistor engaged, waiting for voltage ramp
3. **ENERGIZED** - Drive contactors closed, ready for operation
4. **CHARGING_PRECHARGING** - Charging mode precharge sequence
5. **CHARGING** - Charging mode active

**Key Features:**
- 83% voltage threshold for precharge completion
- 5-second timeout for precharge stability
- Immediate fault response (opens all contactors)
- Proven logic ported from 2024 implementation (~80 lines)

**Interface Functions (to be implemented by other modules):**
- `is_ts_enable_active()` - Tractive system enable status
- `is_fault_present()` - System fault detection
- `is_charge_enable_active()` - Charge mode enable
- `get_tractive_voltage()` - Tractive system voltage reading
- `get_pack_voltage()` - Battery pack voltage reading

### Contactor Control (`hvc_contactors.h/c`)

Direct GPIO control for high voltage contactors:
- **Precharge Contactor** - Controls precharge resistor path
- **AIR+ and AIR-** - Main accumulator isolation relays

**Key Functions:**
- `set_precharge_contactor()` - Control precharge relay
- `set_drive_contactors()` - Control both AIRs together
- `open_all_contactors()` - Emergency shutdown

**⚠️ TODO:** Update pin definitions in `hvc_contactors.c` based on actual HVC schematic.

## Development Approach

### Iterative Testing Strategy

Follow this bottom-up approach to avoid "big jumble of issues":

#### Phase 1: State Machine Validation ✅ **CURRENT**
- [x] Implement state machine with weak function stubs
- [x] Create state machine RTOS task (10Hz)
- [x] Build and verify compilation
- [ ] Flash firmware and verify LED still works
- [ ] Add USB CDC logging to display current state
- [ ] Manually test state transitions via debug interface

#### Phase 2: GPIO Interface
- [ ] Implement real GPIO input readers (HVIL, shutdown sense)
- [ ] Add `is_ts_enable_active()` implementation
- [ ] Test state transitions with real GPIO inputs
- [ ] Add fault detection logic for shutdown pins

#### Phase 3: Voltage Measurements
- [ ] Create measurements task (10Hz)
- [ ] Implement ADC readings (Vsense, Isense, Tsense)
- [ ] Add `get_tractive_voltage()` implementation
- [ ] Add `get_pack_voltage()` implementation
- [ ] Test precharge sequence with real voltage feedback

#### Phase 4: CAN Communication
- [ ] Create CAN task (100Hz)
- [ ] Port VCU communication protocol
- [ ] Add IMD CAN monitoring
- [ ] Test fault propagation via CAN

#### Phase 5: BMS Integration
- [ ] Create BMS/cells task (5Hz)
- [ ] Port isoSPI communication with ADBMS6830 chips
- [ ] Implement cell voltage monitoring (140 cells)
- [ ] Implement temperature monitoring (50 thermistors)
- [ ] Add cell balancing logic

#### Phase 6: Safety & Integration
- [ ] Create safety monitor task (100Hz)
- [ ] Implement fault hysteresis (5s timer)
- [ ] Add comprehensive fault handling
- [ ] Full system integration testing

### Testing Best Practices

1. **One Feature at a Time** - Implement, test, commit
2. **Logging Everything** - Use USB CDC for debug output
3. **Dummy Data** - Use test/dummy implementations before real sensors
4. **Git Workflow** - Create feature branches, merge when working
5. **Hardware Validation** - Test each peripheral independently

### Commit Strategy

Example commit messages:
```
feat(hvc): Add state machine task with weak stubs
feat(hvc): Implement GPIO input readers for TS_Enable
feat(hvc): Add ADC measurements task
fix(hvc): Correct precharge voltage threshold calculation
test(hvc): Validate precharge sequence with oscilloscope
```

## Pin Configuration

### Contactor Control (TODO: Verify with schematic)
| Signal | Pin | Port | Notes |
|--------|-----|------|-------|
| Precharge Contactor | PB0 | GPIOB | **Placeholder - update from schematic** |
| AIR+ | PB1 | GPIOB | **Placeholder - update from schematic** |
| AIR- (Close_IR_+) | PB6 | GPIOB | **Placeholder - currently using available GPIO** |

### Input Signals (Already configured in CubeMX)
| Signal | Pin | Port | Type |
|--------|-----|------|------|
| IR- Sense | PC2 | GPIOC | Digital Input |
| IR+ Sense | PC3 | GPIOC | Digital Input |
| Shutdown Sense 1-4 | PC6-9 | GPIOC | Digital Input |
| Shutdown Sense 12 | PA8 | GPIOA | Digital Input |

### Output Signals (Already configured)
| Signal | Pin | Port | Type |
|--------|-----|------|------|
| BMS_Error | PB0 | GPIOB | Digital Output |
| IMD_Error | PB1 | GPIOB | Digital Output |
| Close_IR_+ | PB6 | GPIOB | Digital Output |

### Analog Inputs (Already configured)
| Signal | Pin | ADC | Notes |
|--------|-----|-----|-------|
| Voltage Sense | PC4 | ADC1 | Tractive system voltage |
| Voltage Sense | PC5 | ADC1 | Additional voltage sense |
| Current Sense | PA6 | ADC2 | Tractive system current |
| Current Sense | PA7 | ADC2 | Additional current sense |
| Temp Sense 1-4 | Various | ADC1/2 | Temperature monitoring |

## RTOS Configuration

### Task Priorities
- **State Machine**: High (osPriorityHigh)
- **Default Task**: Normal (osPriorityNormal)
- **LED Task**: Low (osPriorityLow)
- **DFU Task**: Normal (osPriorityNormal)

### Task Stack Sizes
- **State Machine**: 1024 bytes (256 * 4)
- **Default Task**: 512 bytes (128 * 4)

### Timing
- **State Machine**: 100ms period (10Hz)
- **System Tick**: 1ms (default FreeRTOS)

## Building and Flashing

### Build Commands
```bash
# Build firmware
bazel build //HVC/firmware:hvc_firmware_2026_elf --config=debug

# Flash via OpenOCD
bazel run //HVC/firmware:openocd

# Flash via DFU (press button PB7 during power-up)
bazel run //HVC/firmware:dfu
```

### Debugging
- USB CDC serial output for logging
- Rainbow LED for visual feedback
- State machine state visible via logging

## Code Quality Standards

### Documentation
- Every function has Doxygen-style comments
- File headers include purpose and copyright
- TODO comments for items requiring schematic verification

### Naming Conventions
- Files: `hvc_module_name.h/c`
- Functions: `module_verb_noun()` (e.g., `state_machine_init()`)
- Types: `module_type_t` (e.g., `hvc_state_t`)
- Constants: `MODULE_CONSTANT_NAME` (e.g., `HVC_PRECHARGE_THRESHOLD_PERCENT`)

### Safety
- All contactors default to OPEN on initialization
- Faults immediately trigger safe state
- Weak function stubs return safe defaults (false, 0V)
- State machine validates all transitions

## References

- **2024 Implementation**: `hvc-firmware-2024-main/` directory
- **VCU Reference**: `VCU/firmware/` (working RTOS example)
- **Longhorn Library**: `drivers/longhorn-lib/` (LED, DFU, USB)
- **STM32G4 HAL**: `drivers/stm32g4/`

## Next Steps

1. Flash firmware and verify LED operation
2. Add USB CDC logging for state machine
3. Test state machine with manual flag toggling
4. Implement GPIO input readers
5. Begin Phase 2 testing

---
**Created**: 2025-11-30  
**Status**: Phase 1 Complete - State Machine Foundation  
**Branch**: `hvc-firmware-initial`
