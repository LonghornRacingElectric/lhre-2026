# ADBMS6830 Integration Plan

## Overview
ADI provides reference code in `STM32CubeIDE/ADBMS6830/` that we'll use as our foundation for HVC BMS development.

## ADI Code Structure

### Library Layer (`lib/`)
**Location**: `STM32CubeIDE/ADBMS6830/lib/`

Core BMS driver files (vendor-provided, minimal modification):
- `adBms6830GenericType.c/h` - Low-level SPI communication, PEC/CRC
- `adBms6830ParseCreate.c/h` - Command parsing and packet creation  
- `adBms6830Data.h` - Data structures for all registers
- `adBms6830CmdList.h` - Command definitions

### Application Layer (`program/`)
**Location**: `STM32CubeIDE/ADBMS6830/program/`

Higher-level application code (we'll adapt this):
- `adBms_Application.c/h` - Example usage patterns and test cases
- `mcuWrapper.c/h` - **Hardware abstraction layer (HAL)**
- `serialPrintResult.c/h` - Debug output helpers

## Key Data Structures

### `cell_asic` - Main BMS IC Structure
Located in `adBms6830Data.h`:
```c
typedef struct {
  cfa_ cfa;              // Config Register A (measurement settings)
  cfb_ cfb;              // Config Register B (OV/UV thresholds, discharge)
  cv_ cv;                // Cell Voltages (16 cells)
  acv_ acv;              // Average Cell Voltages
  scv_ scv;              // S Voltages
  fcv_ fcv;              // Filtered Cell Voltages
  ax_ ax;                // Aux Voltages (12 channels - temps, GPIO)
  rax_ rax;              // Redundant Aux Voltages
  sta_ sta;              // Status A (internal refs, temp)
  stb_ stb;              // Status B (supply voltages)
  stc_ stc;              // Status C (fault flags)
  std_ std;              // Status D (OV/UV flags per cell)
  ste_ ste;              // Status E (GPIO states, revision)
  // ... more registers
} cell_asic;
```

### Config Registers
**CFA** - Measurement Control:
- `refon`: Reference voltage enable
- `cth`: Cell threshold for OV/UV detection
- `gpo[10]`: GPIO output states
- `fc[3]`: Filter coefficient for averaging

**CFB** - Protection Thresholds:
- `vuv`: Under-voltage threshold (16-bit)
- `vov`: Over-voltage threshold (16-bit)
- `dcc[16]`: Discharge enable bitmask (cell balancing)
- `dcto`: Discharge timeout

## Integration Strategy

### Phase 1: Hardware Abstraction Layer
**File**: `HVC/firmware/Core/Src/hvc_bms.c`

Adapt `mcuWrapper.c` functions to HVC hardware:
```c
// Current mcuWrapper uses:
extern SPI_HandleTypeDef hspi1;  // ADI example
#define CS_PIN GPIO_PIN_6         // ADI example  
#define GPIO_PORT GPIOB           // ADI example

// HVC uses:
extern SPI_HandleTypeDef hspi4;  // Our hardware
#define CS_PIN GPIO_PIN_3         // PE3
#define GPIO_PORT GPIOE           // GPIOE
```

Key functions to implement:
- `adBmsCsLow()` / `adBmsCsHigh()` - Chip select control
- `spiWriteBytes()` - SPI transmit
- `spiWriteReadBytes()` - SPI full duplex
- `adBmsWakeupIc()` - Send wakeup pulse (dummy bytes)
- `Delay_ms()` - Already have via FreeRTOS `osDelay()`

### Phase 2: Basic Communication
Use ADI's library functions directly:
```c
void bms_init(void) {
  // 1. Wakeup BMS IC
  adBmsWakeupIc(1);  // 1 IC in our validation setup
  
  // 2. Initialize cell_asic structure
  cell_asic ic;
  adBms6830_init_config(1, &ic);
  
  // 3. Write configuration
  adBms6830_write_config(1, &ic);
  
  // 4. Verify config readback
  adBms6830_read_config(1, &ic);
}

void bms_update(void) {
  static cell_asic ic;
  
  // 1. Start cell voltage measurement
  adBms6830_start_adc_cell_voltage_measurment(1);
  
  // 2. Wait for conversion (poll or delay)
  osDelay(10);  // ~10ms typical
  
  // 3. Read cell voltages
  adBms6830_read_cell_voltages(1, &ic);
  
  // 4. Extract voltages from ic.cv.c_codes[0..15]
  for (int i = 0; i < 5; i++) {  // 5 cells in validation
    float voltage = ic.cv.c_codes[i] * 0.0001;  // Convert to volts
    // Store/report voltage
  }
}
```

### Phase 3: Use ADI Application Examples
Reference `adBms_Application.c` for:
- Configuration register setup patterns
- ADC conversion sequences
- Open-wire detection algorithms
- Cell balancing control
- Self-test procedures
- Status register interpretation

## File Organization

### Copy to HVC Project
1. **Library files** (minimal changes):
   ```
   drivers/adbms6830/
   ├── inc/
   │   ├── adBms6830CmdList.h
   │   ├── adBms6830Data.h
   │   ├── adBms6830GenericType.h
   │   └── adBms6830ParseCreate.h
   └── src/
       ├── adBms6830GenericType.c
       └── adBms6830ParseCreate.c
   ```

2. **HVC-specific wrapper** (adapted from mcuWrapper):
   ```
   HVC/firmware/Core/
   ├── Inc/
   │   └── hvc_bms.h          // Public API
   └── Src/
       └── hvc_bms.c          // Implementation using ADI lib
   ```

### Build Integration
Update `HVC/firmware/BUILD.bazel`:
```starlark
extra_srcs = [
    "//drivers/adbms6830:adbms_lib",  # ADI library
],
extra_includes = [
    "drivers/adbms6830/inc",
],
```

## Development Workflow

### Step 1: SPI Communication Test
- Implement `adBmsCsLow/High()` and `spiWriteBytes()`
- Send wakeup sequence
- Verify CS and SPI signals with logic analyzer

### Step 2: Config Register Test  
- Write config with known values
- Read back config
- Compare - should match exactly

### Step 3: Cell Voltage Reading
- Configure ADC settings in CFA register
- Start conversion with `adBms6830_Adcv()`
- Poll for completion
- Read voltages with `adBms6830_read_cell_voltages()`
- Verify with multimeter on physical cells

### Step 4: Status Monitoring
- Read status registers A/B/C/D
- Check fault flags
- Implement error handling

### Step 5: Cell Balancing
- Set discharge bits in CFB.dcc
- Monitor discharge with filtered voltages
- Implement balancing algorithm

## Key ADI Functions to Use

### Measurement Commands
```c
// Start cell voltage ADC conversion
void adBms6830_Adcv(RD rd, CONT cont, DCP dcp, RSTF rstf, OW_C_S owcs);

// Start aux voltage ADC conversion (temps)
void adBms6830_Adax(OW_AUX owaux, PUP pup, CH ch);

// Snapshot measurements (simultaneous sampling)
void adBms6830_Snap();
```

### Data Read Commands
```c
// Read cell voltages from all groups
void adBmsReadData(uint8_t tIC, cell_asic *ic, uint8_t cmd[2], TYPE type, GRP group);

// Or use convenience wrappers:
void adBms6830_read_cell_voltages(uint8_t tIC, cell_asic *ic);
void adBms6830_read_aux_voltages(uint8_t tIC, cell_asic *ic);
void adBms6830_read_status_registers(uint8_t tIC, cell_asic *ic);
```

### Configuration
```c
// Initialize config structure with defaults
void adBms6830_init_config(uint8_t tIC, cell_asic *ic);

// Write config registers to IC
void adBms6830_write_config(uint8_t tIC, cell_asic *ic);

// Read config registers from IC
void adBms6830_read_config(uint8_t tIC, cell_asic *ic);
```

## Hardware Configuration

### SPI4 Settings (HVC)
- Mode: Master
- Clock: 1 MHz (conservative for initial testing)
- Data Size: 8 bits
- CPOL: Low (0)
- CPHA: 1 Edge
- NSS: Software (manual CS control on PE3)

### Chip Select
- Pin: PE3 (CS for first BMB)
- Active: LOW
- Idle: HIGH

### Wakeup Sequence
Per datasheet, send dummy bytes on SPI to wake from sleep:
```c
void adBmsWakeupIc(uint8_t total_ic) {
  for (int i = 0; i < total_ic; i++) {
    adBmsCsLow();
    // Send dummy byte
    uint8_t dummy = 0xFF;
    spiWriteBytes(1, &dummy);
    adBmsCsHigh();
    Delay_ms(1);  // tWAKE delay
  }
}
```

## Validation Setup
- **1 ADBMS6830 chip** (TOTAL_IC = 1)
- **5 cells connected** (cells 0-4)
- **SPI4** on STM32G474
- **CS** on PE3

## Next Steps
1. Copy ADI library files to `drivers/adbms6830/`
2. Create Bazel build rule for ADBMS library
3. Adapt `mcuWrapper.c` functions to HVC hardware
4. Implement basic `bms_init()` and `bms_update()` 
5. Test with serial output showing cell voltages
6. Validate against multimeter readings

## References
- ADI Code: `STM32CubeIDE/ADBMS6830/`
- Datasheet: ADBMS6830 (for command details, timing)
- Application Code: `program/src/adBms_Application.c` (usage examples)
