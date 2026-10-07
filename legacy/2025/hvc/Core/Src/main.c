/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "fdcan.h"
#include "memorymap.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "usb_device.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdarg.h>
#include <stdio.h>

#include "led.h"
#include "timer.h"
#include "dfu.h"
#include "hvc_can.h"
#include "longhorn/can/can_ids.h"
#include "usb_vcp.h"
#include "vct_sense.h"
#include "state_machine.h"
#include "state_machine_logic.h"
#include "imd.h"
#include "bmb_debug.h"
#include "contactors.h"
#include "cells.h"
#include "faults.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define BMS_PRINT_INTERVAL_MS 1000U
#define USB_PRINT_SETTLE_MS 5U
#define BMS_PRINT_LINE_SIZE 256U
#ifndef HVC_BMS_FAULT_MONITOR_ONLY
#define HVC_BMS_FAULT_MONITOR_ONLY 0
#endif
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
/* USER CODE BEGIN PFP */
static size_t appendToLine(char *line, size_t capacity, size_t length,
                           const char *format, ...);
static size_t appendFaultName(char *line, size_t capacity, size_t length,
                              uint32_t faults, uint32_t fault,
                              const char *name);
static void serviceImd(void);
static void settleUsbAndServiceCan(void);
static void printBmsFaultDiagnostics(void);
static void printHighImpedanceDiagnostics(void);
static void printPackAndBmsSummary(void);
static void printBmsReadings(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static size_t appendToLine(char *line, size_t capacity, size_t length,
                           const char *format, ...)
{
  if (length >= capacity) return capacity - 1U;

  va_list args;
  va_start(args, format);
  int written = vsnprintf(line + length, capacity - length, format, args);
  va_end(args);

  if (written < 0) return length;
  if ((size_t)written >= capacity - length) return capacity - 1U;
  return length + (size_t)written;
}

static size_t appendFaultName(char *line, size_t capacity, size_t length,
                              uint32_t faults, uint32_t fault,
                              const char *name)
{
  if ((faults & fault) == 0U) return length;
  return appendToLine(line, capacity, length, "%s%s",
                      length == 0U ? "" : "|", name);
}

static void serviceImd(void)
{
  imd_can_periodic(isPosContactorClosed() && isNegContactorClosed());
}

static void settleUsbAndServiceCan(void)
{
  HAL_Delay(USB_PRINT_SETTLE_MS);
  hvc_can_service_tx();
  /* The status print blocks the main loop for a while; keep IMD fault
     handling running through it. */
  serviceImd();
}

static void printHighImpedanceDiagnostics(void)
{
  char line[BMS_PRINT_LINE_SIZE];
  const uint32_t suspectCount = getHighImpedanceSuspectCount();

  usb_printf("HIGH_Z bypass=%s suspects=%lu thresholds=<%.2fV/>%.2fV",
             HVC_BRINGUP_IGNORE_HIGH_IMPEDANCE_CELL_FAULTS ? "ON" : "OFF",
             (unsigned long)suspectCount,
             (double)HVC_HIGH_IMPEDANCE_SUSPECT_LOW_MAX_V,
             (double)HVC_HIGH_IMPEDANCE_SUSPECT_HIGH_MIN_V);
  settleUsbAndServiceCan();

  for (uint32_t bmb = 0U; bmb < NUM_BMS_ICS; bmb++)
  {
    size_t length = 0U;
    uint32_t bmbSuspectCount = 0U;
    const uint32_t firstCell = bmb * CELLS_PER_BMB;

    for (uint32_t channel = 0U; channel < CELLS_PER_BMB; channel++)
    {
      const uint32_t cell = firstCell + channel;
      if (!isCellHighImpedanceSuspect(cell)) continue;

      if (bmbSuspectCount == 0U)
      {
        length = appendToLine(line, sizeof(line), length,
                              "HIGH_Z BMB%02lu:",
                              (unsigned long)(bmb + 1U));
      }
      length = appendToLine(line, sizeof(line), length,
                            " C%03lu(ch%02lu)=%.3fV",
                            (unsigned long)(cell + 1U),
                            (unsigned long)(channel + 1U),
                            (double)getCellVoltage(cell));
      bmbSuspectCount++;
    }

    if (bmbSuspectCount != 0U)
    {
      println(line);
      settleUsbAndServiceCan();
    }
  }
}

static void printBmsFaultDiagnostics(void)
{
  char line[BMS_PRINT_LINE_SIZE];
  const uint32_t faults = get_last_faults();
  uint32_t rawFaults = 0U;
  size_t length = 0U;

  if (getNumResponsiveChips() != NUM_BMS_ICS) rawFaults |= FAULT_BMS_COMMS;
  if (hasCellOvervoltage()) rawFaults |= FAULT_BMS_OVERVOLTAGE;
  if (hasCellUndervoltage()) rawFaults |= FAULT_BMS_UNDERVOLTAGE;
  if (hasCellOvertemperature()) rawFaults |= FAULT_BMS_OVERTEMP;

  length = appendFaultName(line, sizeof(line), length, faults,
                           FAULT_BMS_COMMS, "COMMS");
  length = appendFaultName(line, sizeof(line), length, faults,
                           FAULT_BMS_OVERVOLTAGE, "CELL_OV");
  length = appendFaultName(line, sizeof(line), length, faults,
                           FAULT_BMS_UNDERVOLTAGE, "CELL_UV");
  length = appendFaultName(line, sizeof(line), length, faults,
                           FAULT_BMS_OVERTEMP, "TEMP_OT");
  if (length == 0U) length = appendToLine(line, sizeof(line), 0U, "NONE");

  usb_printf("BMS FAULTS active(after delay)=0x%02lX [%s] output=%s state_machine=%s",
             (unsigned long)faults, line,
             HVC_BMS_FAULT_MONITOR_ONLY ? "SUPPRESSED" : "ENABLED",
             HVC_BMS_FAULT_MONITOR_ONLY ? "IGNORED" : "ENABLED");
  settleUsbAndServiceCan();

  length = 0U;
  length = appendFaultName(line, sizeof(line), length, rawFaults,
                           FAULT_BMS_COMMS, "COMMS");
  length = appendFaultName(line, sizeof(line), length, rawFaults,
                           FAULT_BMS_OVERVOLTAGE, "CELL_OV");
  length = appendFaultName(line, sizeof(line), length, rawFaults,
                           FAULT_BMS_UNDERVOLTAGE, "CELL_UV");
  length = appendFaultName(line, sizeof(line), length, rawFaults,
                           FAULT_BMS_OVERTEMP, "TEMP_OT");
  if (length == 0U) length = appendToLine(line, sizeof(line), 0U, "NONE");

  usb_printf("BMS RAW now=0x%02lX [%s] (delays: comms 1s, OV/UV 10s, OT 5s)",
             (unsigned long)rawFaults, line);
  settleUsbAndServiceCan();

  printHighImpedanceDiagnostics();

  if ((rawFaults & FAULT_BMS_COMMS) != 0U)
  {
    length = appendToLine(line, sizeof(line), 0U, "BMS COMMS MISSING:");
    bool first = true;
    for (uint32_t bmb = 0; bmb < NUM_BMS_ICS; bmb++)
    {
      if (isBmbReadingOk(bmb)) continue;
      length = appendToLine(line, sizeof(line), length, "%s BMB%02lu",
                            first ? "" : ",", (unsigned long)(bmb + 1U));
      first = false;
    }
    println(line);
    settleUsbAndServiceCan();
  }
}

static void printPackAndBmsSummary(void)
{
  const float vSense = getTractiveVoltage();
  const float allCellSum = getPackVoltageFromCells();
  const float prechargeTarget =
      allCellSum * HVC_PRECHARGE_THRESHOLD_PERCENT;
  const bool prechargeVoltageOk = vSense > prechargeTarget;
  const float vSensePercent = allCellSum > 1.0f
      ? (vSense / allCellSum) * 100.0f
      : 0.0f;
  float validCellSum = 0.0f;
  float minCell = 0.0f;
  float maxCell = 0.0f;
  float minTemperature = 0.0f;
  float maxTemperature = 0.0f;
  uint32_t minCellIndex = 0U;
  uint32_t maxCellIndex = 0U;
  uint32_t minTemperatureIndex = 0U;
  uint32_t maxTemperatureIndex = 0U;
  uint32_t validCells = 0U;
  uint32_t validTemperatures = 0U;

  for (uint32_t cell = 0U; cell < NUM_BMS_ICS * CELLS_PER_BMB; cell++)
  {
    const uint32_t bmb = cell / CELLS_PER_BMB;
    if (!isBmbReadingOk(bmb) || !isCellVoltageReadingOk(cell)) continue;

    const float voltage = getCellVoltage(cell);
    validCellSum += voltage;
    if (validCells == 0U || voltage < minCell)
    {
      minCell = voltage;
      minCellIndex = cell;
    }
    if (validCells == 0U || voltage > maxCell)
    {
      maxCell = voltage;
      maxCellIndex = cell;
    }
    validCells++;
  }

  for (uint32_t temperature = 0U;
       temperature < NUM_BMS_ICS * TEMPERATURES_PER_BMB;
       temperature++)
  {
    const uint32_t bmb = temperature / TEMPERATURES_PER_BMB;
    if (!isBmbReadingOk(bmb) ||
        !isCellTemperatureReadingOk(temperature)) continue;

    const float value = getCellTemperature(temperature);
    if (validTemperatures == 0U || value < minTemperature)
    {
      minTemperature = value;
      minTemperatureIndex = temperature;
    }
    if (validTemperatures == 0U || value > maxTemperature)
    {
      maxTemperature = value;
      maxTemperatureIndex = temperature;
    }
    validTemperatures++;
  }

  usb_printf("HV BUS_VSENSE=%.2fV PACK_CELL_SUM_ALL=%.2fV DELTA=%.2fV RATIO=%.1f%% VALID_SUM=%.2fV",
             (double)vSense, (double)allCellSum,
             (double)(vSense - allCellSum), (double)vSensePercent,
             (double)validCellSum);
  settleUsbAndServiceCan();

  /* Cells with no BMS reading (dead channels and the removed BMB). With both
     AIRs closed BUS_VSENSE is the whole pack, so the gap to the measured sum
     is what the unmonitored cells hold between them. */
  const uint32_t unmonitoredCells = PACK_SERIES_CELLS - getMeasuredCellCount();
  if (unmonitoredCells > 0U && isPosContactorClosed() && isNegContactorClosed())
  {
    const float unmonitoredVoltage = vSense - getMeasuredCellSum();
    usb_printf("UNMONITORED cells=%lu est_avg=%.3fV/cell (BUS_VSENSE %.1fV - measured %.1fV) charging=%s",
               (unsigned long)unmonitoredCells,
               (double)(unmonitoredVoltage / (float)unmonitoredCells),
               (double)vSense, (double)getMeasuredCellSum(),
               HVC_DISABLE_CHARGING ? "DISABLED" : "ENABLED");
  }
  else
  {
    usb_printf("UNMONITORED cells=%lu est_avg=n/a (needs both AIRs closed) charging=%s%s",
               (unsigned long)unmonitoredCells,
               HVC_DISABLE_CHARGING ? "DISABLED" : "ENABLED",
               isChargingBlocked() ? " CHARGER_CONNECTED->HELD_OFF" : "");
  }
  settleUsbAndServiceCan();

  usb_printf("PRECHARGE target=%.2fV (%u%% of CELL_SUM_ALL) gate=%s qualified=%lums/%ums",
             (double)prechargeTarget,
             (unsigned int)(HVC_PRECHARGE_THRESHOLD_PERCENT * 100.0f),
             prechargeVoltageOk ? "PASS" : "HOLD",
             (unsigned long)(prechargeVoltageOk
                 ? get_precharge_qualified_ms()
                 : 0U),
             (unsigned int)HVC_PRECHARGE_VALID_MS);
  settleUsbAndServiceCan();

  if (validCells > 0U)
  {
    usb_printf("CELLS valid=%lu/%u invalid=%lu min=C%03lu %.3fV max=C%03lu %.3fV",
               (unsigned long)validCells,
               (unsigned int)(NUM_BMS_ICS * CELLS_PER_BMB),
               (unsigned long)(NUM_BMS_ICS * CELLS_PER_BMB - validCells),
               (unsigned long)(minCellIndex + 1U), (double)minCell,
               (unsigned long)(maxCellIndex + 1U), (double)maxCell);
  }
  else
  {
    usb_printf("CELLS valid=0/%u invalid=%u min=NONE max=NONE",
               (unsigned int)(NUM_BMS_ICS * CELLS_PER_BMB),
               (unsigned int)(NUM_BMS_ICS * CELLS_PER_BMB));
  }
  settleUsbAndServiceCan();

  if (validTemperatures > 0U)
  {
    usb_printf("TEMPS valid=%lu/%u invalid=%lu min=T%03lu %.1fC max=T%03lu %.1fC",
               (unsigned long)validTemperatures,
               (unsigned int)(NUM_BMS_ICS * TEMPERATURES_PER_BMB),
               (unsigned long)(NUM_BMS_ICS * TEMPERATURES_PER_BMB -
                               validTemperatures),
               (unsigned long)(minTemperatureIndex + 1U),
               (double)minTemperature,
               (unsigned long)(maxTemperatureIndex + 1U),
               (double)maxTemperature);
  }
  else
  {
    usb_printf("TEMPS valid=0/%u invalid=%u min=NONE max=NONE",
               (unsigned int)(NUM_BMS_ICS * TEMPERATURES_PER_BMB),
               (unsigned int)(NUM_BMS_ICS * TEMPERATURES_PER_BMB));
  }
  settleUsbAndServiceCan();
}

static void printBmsReadings(void)
{
  hvc_can_rx_status_t canRx;
  hvc_can_tx_status_t canTx;

  hvc_can_get_rx_status(&canRx);
  hvc_can_get_tx_status(&canTx);

  usb_printf("HVC state=%s shutdown=%s AIR_SENSE=[+%u,-%u] faults=0x%02lX latched=0x%02lX",
             get_state_name(get_current_state()),
             isShutdownClosed() ? "CLOSED" : "OPEN",
             (unsigned int)isPosContactorClosed(),
             (unsigned int)isNegContactorClosed(),
             (unsigned long)get_last_faults(),
             (unsigned long)get_latched_faults());
  settleUsbAndServiceCan();

  usb_printf("IMD OK_SIGNAL=%.2fV(%s) STATE=%s for=%lums TRIP=%s LATCH_SET(PB1)=%u%s",
             (double)getImdOkVoltage(),
             isImdSignalOk() ? "OK" : "FAULT",
             imd_mode_policy_state_name(getImdState()),
             (unsigned long)getImdStateAgeMs(),
             imd_mode_policy_trip_name(getImdTripReason()),
             (unsigned int)isImdLatchSetAsserted(),
             isImdBypassed() ? " BYPASS(PB1 forced 0)" : "");
  settleUsbAndServiceCan();

  usb_printf("SAFETY IMD=%s VCU_RX=%s age=%lums PRNDL=%u SHDN=[%u,%u,%u,%u] SHDN12_24V=%s AIR_SENSE=[+%u,-%u]",
             isImdOk() ? "OK" : "TRIPPED",
             canRx.vcuStateValid ? "OK" : "TIMEOUT",
             (unsigned long)canRx.vcuStateAgeMs,
             (unsigned int)canRx.prndlState,
             (unsigned int)isShutdownOneOk(),
             (unsigned int)isShutdownTwoOk(),
             (unsigned int)isShutdownThreeOk(),
             (unsigned int)isShutdownFourOk(),
             isShutdownTwelveOk() ? "ON" : "OFF",
             (unsigned int)isPosContactorClosed(),
             (unsigned int)isNegContactorClosed());
  settleUsbAndServiceCan();

  usb_printf("CAN TX 0x%03X=[%02X %02X %02X %02X] mode=%s started=%u registered=%u age=%lums queued=%lu dropped=%lu",
             (unsigned int)CONTACTOR_STATUS_ID,
             (unsigned int)canTx.contactorStatusData[0],
             (unsigned int)canTx.contactorStatusData[1],
             (unsigned int)canTx.contactorStatusData[2],
             (unsigned int)canTx.contactorStatusData[3],
             canTx.contactorStateForced ? "FORCED_ENERGIZED" : "LIVE",
             (unsigned int)canTx.interfaceStarted,
             (unsigned int)canTx.contactorStatusRegistered,
             (unsigned long)canTx.contactorStatusAgeMs,
             (unsigned long)canTx.messagesQueued,
             (unsigned long)canTx.droppedPackets);
  settleUsbAndServiceCan();

  printPackAndBmsSummary();
  printBmsFaultDiagnostics();
  /* BENCH: raw ADBMS register dump of the BMB under test (see bmb_debug.h). */
  bmb_debug_dump(settleUsbAndServiceCan);

  char line[BMS_PRINT_LINE_SIZE];
  usb_printf("ADBMS scan: %lu/%u BMBs OK | ! = bad read; trips: cell <%.2fV/>%.2fV, temp >60C",
             (unsigned long)getNumResponsiveChips(), (unsigned int)NUM_BMS_ICS,
             (double)CELL_UNDER_VOLTAGE, (double)CELL_OVER_VOLTAGE);
  settleUsbAndServiceCan();

  for (uint32_t bmb = 0; bmb < NUM_BMS_ICS; bmb++)
  {
    const bool communicationsOk = isBmbReadingOk(bmb);
    bool cellsOk = communicationsOk;
    bool temperaturesOk = communicationsOk;
    const uint32_t firstCell = bmb * CELLS_PER_BMB;
    const uint32_t firstTemperature = bmb * TEMPERATURES_PER_BMB;

    for (uint32_t channel = 0; channel < CELLS_PER_BMB; channel++)
    {
      cellsOk &= isCellVoltageReadingOk(firstCell + channel);
    }
    for (uint32_t channel = 0; channel < TEMPERATURES_PER_BMB; channel++)
    {
      temperaturesOk &= isCellTemperatureReadingOk(firstTemperature + channel);
    }

    const char *cellStatus = !communicationsOk ? "CRC_FAULT" :
                             (cellsOk ? "OK" : "RANGE_FAULT");
    size_t length = appendToLine(line, sizeof(line), 0U,
                                 "BMB%02lu CELLS %s:",
                                 (unsigned long)(bmb + 1U), cellStatus);
    for (uint32_t channel = 0; channel < CELLS_PER_BMB; channel++)
    {
      const uint32_t cellIndex = firstCell + channel;
      const bool readingOk = communicationsOk && isCellVoltageReadingOk(cellIndex);
      length = appendToLine(line, sizeof(line), length, " C%03lu=%s%.3f",
                            (unsigned long)(cellIndex + 1U), readingOk ? "" : "!",
                            (double)getCellVoltage(cellIndex));
    }
    println(line);
    settleUsbAndServiceCan();

    const char *temperatureStatus = !communicationsOk ? "CRC_FAULT" :
                                    (temperaturesOk ? "OK" : "RANGE_FAULT");
    length = appendToLine(line, sizeof(line), 0U,
                          "BMB%02lu TEMPS %s:",
                          (unsigned long)(bmb + 1U), temperatureStatus);
    for (uint32_t channel = 0; channel < TEMPERATURES_PER_BMB; channel++)
    {
      const uint32_t temperatureIndex = firstTemperature + channel;
      const bool readingOk = communicationsOk &&
                             isCellTemperatureReadingOk(temperatureIndex);
      length = appendToLine(line, sizeof(line), length, " T%03lu=%s%.1f",
                            (unsigned long)(temperatureIndex + 1U), readingOk ? "" : "!",
                            (double)getCellTemperature(temperatureIndex));
    }
    println(line);
    settleUsbAndServiceCan();
  }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* Configure the peripherals common clocks */
  PeriphCommonClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_UART5_Init();
  MX_FDCAN1_Init();
  MX_FDCAN3_Init();
  MX_SPI3_Init();
  MX_TIM2_Init();
  MX_TIM8_Init();
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN 2 */

  HAL_TIM_Base_Start(&htim8);

  led_init(TIM2, &htim2, 3);
  lib_timer_init();
  dfu_init(BOOT0trig_GPIO_Port, BOOT0trig_Pin);
  hvc_can_init();
  imd_can_init();
  cells_init();
  contactors_init();
  faults_init();
  state_machine_init();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  static uint32_t lastBmsPrintTime = 0;
  static uint32_t lastStateMachineTime = 0;
  static uint32_t deltaTime;
  static uint32_t currentFaults = 0;
  static bool bmsIndicatorError = false;
  static bool imdIndicatorError = false;
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    deltaTime = lib_timer_delta_ms();
    const uint32_t currentTime = lib_timer_elapsed_ms();

    /* Every loop: PA3 (IMD OK) and the AIR feedback decide PB1 (latch set). */
    serviceImd();

    /* Service safety-critical CAN immediately before the one blocking ADBMS
       conversion step as well as after it in hvc_can_periodic(). */
    hvc_can_service_tx();
    cells_periodic((int)get_current_state());

    if ((uint32_t)(currentTime - lastStateMachineTime) >= 100U)
    {
      lastStateMachineTime = currentTime;
      currentFaults = get_faults();
#if HVC_BMS_FAULT_MONITOR_ONLY
      setBmsError(false);
#else
      latch_faults(currentFaults);
      setBmsError(currentFaults != 0U);
#endif

      const bool startupComplete = currentTime > 8000U;
      if (startupComplete)
      {
#if !HVC_BMS_FAULT_MONITOR_ONLY
        bmsIndicatorError = bmsIndicatorError || currentFaults != 0U;
#endif
        imdIndicatorError = imdIndicatorError || !isImdOk();
      }

      /* An IMD trip blocks precharge until power cycle, so the reset button
         cannot re-energize into an insulation fault. */
      const bool anyFaults =
#if HVC_BMS_FAULT_MONITOR_ONLY
          !startupComplete || !isImdOk();
#else
          get_latched_faults() != 0U || !startupComplete || !isImdOk();
#endif
      update_state_machine(anyFaults);
    }

    hvc_can_periodic(bmsIndicatorError, imdIndicatorError,
                     (int)get_current_state(),
                     (float)deltaTime / 1000.0f);
    receive_periodic();

    if ((uint32_t)(currentTime - lastBmsPrintTime) >= BMS_PRINT_INTERVAL_MS)
    {
      lastBmsPrintTime = currentTime;
      printBmsReadings();
    }

    led_rainbow(deltaTime / 1000.0f);

    //timer += deltaTime / 1000.0f;
    /*
    if(timer >= 3.0f) {
      usb_printf("HV Voltage: %.3f", getTractiveVoltage());
      usb_printf("HV Current: &.3f", getTractiveCurrent());
      usb_printf("IMD Status: %d", (int) isImdOk());
      timer = 0.0f;
    }
    */
    //HAL_Delay(1);
  }

  HAL_Delay(1);
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 2;
  /* 16 MHz HSE / 2 * 68 = 544 MHz SYSCLK. */
  RCC_OscInitStruct.PLL.PLLN = 68;
  RCC_OscInitStruct.PLL.PLLP = 1;
  RCC_OscInitStruct.PLL.PLLQ = 1;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Peripherals Common Clock Configuration
  * @retval None
  */
void PeriphCommonClock_Config(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Initializes the peripherals clock
  */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_ADC|RCC_PERIPHCLK_FDCAN;
  PeriphClkInitStruct.PLL2.PLL2M = 2;
  /* 16 MHz HSE / 2 * 25 / 2 = 100 MHz for FDCAN and ADC. */
  PeriphClkInitStruct.PLL2.PLL2N = 25;
  PeriphClkInitStruct.PLL2.PLL2P = 2;
  PeriphClkInitStruct.PLL2.PLL2Q = 2;
  PeriphClkInitStruct.PLL2.PLL2R = 2;
  PeriphClkInitStruct.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_3;
  PeriphClkInitStruct.PLL2.PLL2VCOSEL = RCC_PLL2VCOWIDE;
  PeriphClkInitStruct.PLL2.PLL2FRACN = 0;
  PeriphClkInitStruct.FdcanClockSelection = RCC_FDCANCLKSOURCE_PLL2;
  PeriphClkInitStruct.AdcClockSelection = RCC_ADCCLKSOURCE_PLL2;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
