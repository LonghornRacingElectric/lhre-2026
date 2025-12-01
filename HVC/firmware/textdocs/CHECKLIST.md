# HVC Development Checklist

## ✅ Phase 1: State Machine Foundation (COMPLETED)

### Module Implementation
- [x] Created `App/Inc/` and `App/Src/` directory structure
- [x] Implemented `hvc_state_machine.h` with clean API
- [x] Implemented `hvc_state_machine.c` with 5-state FSM
- [x] Implemented `hvc_contactors.h` with contactor control API
- [x] Implemented `hvc_contactors.c` with GPIO control
- [x] Added weak function stubs for sensor interfaces
- [x] Documented all functions with Doxygen comments

### RTOS Integration
- [x] Created state machine task in `app_freertos.c`
- [x] Configured task at 10Hz (100ms period)
- [x] Set high priority for state machine task
- [x] Added proper initialization sequence
- [x] Maintained LED and DFU functionality

### Build System
- [x] Updated `BUILD.bazel` with `extra_srcs` and `extra_includes`
- [x] Successfully compiled firmware
- [x] Generated `.elf` and `.bin` artifacts
- [x] No compilation errors or warnings

### Documentation
- [x] Created comprehensive `App/README.md`
- [x] Documented architecture overview
- [x] Created development roadmap
- [x] Added pin configuration reference
- [x] Included testing best practices

## 🔄 Phase 2: GPIO Interface (NEXT)

### Requirements
- [ ] **Flash and test** current firmware
  - [ ] Verify LED rainbow still works
  - [ ] Verify DFU mode entry works
  - [ ] Confirm firmware runs without crashes

### Implementation Tasks
- [ ] **Add USB CDC logging**
  - [ ] Create logging helper functions
  - [ ] Log state machine state on every transition
  - [ ] Log contactor states when they change
  - [ ] Add periodic heartbeat log (every 1 second)

- [ ] **Implement GPIO input readers**
  - [ ] Create `hvc_gpio.h` module
  - [ ] Create `hvc_gpio.c` implementation
  - [ ] Add `read_shutdown_sense_pins()` function
  - [ ] Add `read_hvil_status()` function
  - [ ] Add debouncing logic (10ms minimum stable time)

- [ ] **Implement sensor interface functions**
  - [ ] Replace weak `is_ts_enable_active()` with real GPIO read
  - [ ] Define which pin is TS_Enable (check schematic)
  - [ ] Add proper GPIO initialization

- [ ] **Testing**
  - [ ] Build and flash firmware
  - [ ] Monitor USB CDC output
  - [ ] Manually toggle GPIO inputs
  - [ ] Verify state transitions occur correctly
  - [ ] Test fault response (open all contactors immediately)

### Success Criteria
- ✅ State machine responds to real GPIO inputs
- ✅ Logging shows state transitions clearly
- ✅ Fault detection works reliably
- ✅ No spurious state changes (debouncing works)

## 📋 Phase 3: Voltage Measurements (FUTURE)

### Requirements
- [ ] Phase 2 must be complete and tested

### Implementation Tasks
- [ ] **Create measurements task**
  - [ ] Add task definition in `app_freertos.c`
  - [ ] Set 10Hz update rate
  - [ ] Set medium priority

- [ ] **Implement ADC module**
  - [ ] Create `hvc_measurements.h` module
  - [ ] Create `hvc_measurements.c` implementation
  - [ ] Add `read_voltage_sense()` function
  - [ ] Add `read_current_sense()` function
  - [ ] Add `read_temperature_sense()` function
  - [ ] Implement voltage scaling/calibration

- [ ] **Integrate with state machine**
  - [ ] Replace weak `get_tractive_voltage()` with ADC read
  - [ ] Replace weak `get_pack_voltage()` with ADC read
  - [ ] Add thread-safe data sharing (mutex or message queue)

- [ ] **Testing**
  - [ ] Verify ADC readings via USB CDC log
  - [ ] Compare with multimeter/oscilloscope
  - [ ] Test precharge sequence with real voltage feedback
  - [ ] Validate 83% threshold works correctly
  - [ ] Measure precharge ramp time

### Success Criteria
- ✅ ADC readings are accurate (within 1%)
- ✅ Precharge sequence completes successfully
- ✅ Voltage threshold detection is reliable
- ✅ No race conditions in data sharing

## 📋 Phase 4: CAN Communication (FUTURE)

### Requirements
- [ ] Phase 3 must be complete and tested

### Implementation Tasks
- [ ] **Create CAN task**
  - [ ] Add task definition (100Hz)
  - [ ] Set high priority

- [ ] **Implement CAN module**
  - [ ] Create `hvc_can.h` module
  - [ ] Create `hvc_can.c` implementation
  - [ ] Port VCU communication protocol from 2024
  - [ ] Add IMD CAN monitoring
  - [ ] Implement message parsing

- [ ] **Integrate with state machine**
  - [ ] Add CAN-based state command interface (for testing)
  - [ ] Send state machine status via CAN
  - [ ] Send voltage/current/temp via CAN

- [ ] **Testing**
  - [ ] Verify CAN messages with CANalyzer/PCAN
  - [ ] Test communication with VCU
  - [ ] Test IMD message reception
  - [ ] Validate message timing (100Hz)

### Success Criteria
- ✅ CAN bus operates at 500kbps
- ✅ Messages sent/received reliably
- ✅ VCU communication works
- ✅ IMD monitoring functional

## 📋 Phase 5: BMS Integration (FUTURE)

### Requirements
- [ ] Phase 4 must be complete and tested

### Implementation Tasks
- [ ] **Create BMS task**
  - [ ] Add task definition (5Hz)
  - [ ] Set medium-high priority

- [ ] **Implement BMS module**
  - [ ] Port 2024 `cells.cpp` logic
  - [ ] Create `hvc_bms.h` module
  - [ ] Create `hvc_bms.c` implementation
  - [ ] Implement isoSPI communication
  - [ ] Add cell voltage reading (140 cells)
  - [ ] Add temperature reading (50 thermistors)
  - [ ] Implement cell balancing logic

- [ ] **Integrate with state machine**
  - [ ] Add BMS fault detection
  - [ ] Replace weak `is_fault_present()` with BMS checks
  - [ ] Add cell voltage bounds checking
  - [ ] Add temperature bounds checking

- [ ] **Testing**
  - [ ] Verify isoSPI communication
  - [ ] Read all cell voltages
  - [ ] Read all thermistor values
  - [ ] Test balancing algorithm
  - [ ] Validate fault detection

### Success Criteria
- ✅ All 140 cell voltages read correctly
- ✅ All 50 temperatures read correctly
- ✅ isoSPI communication is reliable
- ✅ Balancing works as expected
- ✅ Fault detection triggers state machine response

## 📋 Phase 6: Safety & Integration (FUTURE)

### Requirements
- [ ] All previous phases complete

### Implementation Tasks
- [ ] **Create safety monitor task**
  - [ ] Add task definition (100Hz)
  - [ ] Set highest priority

- [ ] **Implement comprehensive fault handling**
  - [ ] Port 2024 fault logic
  - [ ] Add 5-second hysteresis timer
  - [ ] Add fault latching
  - [ ] Add fault clearing logic

- [ ] **Full system integration**
  - [ ] Test all tasks running together
  - [ ] Verify task timing with oscilloscope
  - [ ] Measure CPU utilization
  - [ ] Test under load conditions

- [ ] **System validation**
  - [ ] Complete precharge cycle test
  - [ ] Drive mode operation test
  - [ ] Charging mode operation test
  - [ ] Emergency shutdown test
  - [ ] Fault recovery test

### Success Criteria
- ✅ All tasks run within timing requirements
- ✅ CPU utilization < 50%
- ✅ No stack overflows
- ✅ Fault handling is robust
- ✅ System passes all safety tests

## 🎯 Current Status

**Phase**: 1 (State Machine Foundation)  
**Status**: ✅ **COMPLETE**  
**Next Action**: Flash firmware and verify LED + DFU functionality  
**Branch**: `hvc-firmware-initial`

## 🔧 Immediate TODO List

1. **Flash and Validate**
   ```bash
   bazel run //HVC/firmware:dfu
   # Or with OpenOCD:
   bazel run //HVC/firmware:openocd
   ```

2. **Verify Basic Functionality**
   - LED should display rainbow
   - DFU button (PB7) should enter bootloader mode
   - No crashes or hard faults

3. **Add USB CDC Logging**
   - Create simple logging module
   - Log "HVC State Machine Init" on startup
   - Log current state every second

4. **Begin Phase 2 Implementation**
   - Start with GPIO input module
   - Test with real hardware inputs

## 📝 Notes

### Pin Mapping Verification Needed
The following pins in `hvc_contactors.c` are **placeholders**:
- `PRECHARGE_CONTACTOR_PIN` (currently PB0)
- `AIR_PLUS_PIN` (currently PB1)
- `AIR_MINUS_PIN` (currently PB6)

**Action**: Review HVC schematic and update pin assignments before hardware testing with actual contactors.

### Testing Without High Voltage
For development without high voltage:
1. Use LED output as visual feedback for contactor states
2. Use potentiometers on ADC inputs for voltage simulation
3. Use switches/buttons for GPIO input simulation
4. Test state transitions via CAN commands

### Known IntelliSense Issues
IntelliSense shows errors in several files, but these are false positives. The build succeeds without errors. Common IntelliSense issues:
- `app_freertos.c`: Expected ')' errors
- `main.c`: HAL function parameter errors

These can be safely ignored as the GCC compiler has no issues.

---
**Last Updated**: 2025-11-30  
**Updated By**: State machine foundation implementation  
**Next Review**: After Phase 2 completion
