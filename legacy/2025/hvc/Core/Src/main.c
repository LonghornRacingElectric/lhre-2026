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
static void settleUsbAndServiceCan(void);
static void printBmsFaultDiagnostics(void);
static void printPackAndBmsSummary(void);
static void printCellRows(void);
static void printTemperatureRows(void);
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

static void settleUsbAndServiceCan(void)
{
  HAL_Delay(USB_PRINT_SETTLE_MS);
  hvc_can_service_tx();
}

static void printBmsFaultDiagnostics(void)
{
  char activeNames[80];
  char latchedNames[80];
  const uint32_t activeFaults = get_last_faults();
  const uint32_t latchedFaults = get_latched_faults();
  size_t length = 0U;

  length = appendFaultName(activeNames, sizeof(activeNames), length,
                           activeFaults,
                           FAULT_BMS_COMMS, "COMMS");
  length = appendFaultName(activeNames, sizeof(activeNames), length,
                           activeFaults,
                           FAULT_BMS_OVERVOLTAGE, "CELL_OV");
  length = appendFaultName(activeNames, sizeof(activeNames), length,
                           activeFaults,
                           FAULT_BMS_UNDERVOLTAGE, "CELL_UV");
  length = appendFaultName(activeNames, sizeof(activeNames), length,
                           activeFaults,
                           FAULT_BMS_OVERTEMP, "TEMP_OT");
  if (length == 0U) {
    appendToLine(activeNames, sizeof(activeNames), 0U, "NONE");
  }

  length = 0U;
  length = appendFaultName(latchedNames, sizeof(latchedNames), length,
                           latchedFaults,
                           FAULT_BMS_COMMS, "COMMS");
  length = appendFaultName(latchedNames, sizeof(latchedNames), length,
                           latchedFaults,
                           FAULT_BMS_OVERVOLTAGE, "CELL_OV");
  length = appendFaultName(latchedNames, sizeof(latchedNames), length,
                           latchedFaults,
                           FAULT_BMS_UNDERVOLTAGE, "CELL_UV");
  length = appendFaultName(latchedNames, sizeof(latchedNames), length,
                           latchedFaults,
                           FAULT_BMS_OVERTEMP, "TEMP_OT");
  if (length == 0U) {
    appendToLine(latchedNames, sizeof(latchedNames), 0U, "NONE");
  }

  usb_printf("BMS GPIO=%s active=0x%02lX[%s] latched=0x%02lX[%s] delays=[COMMS 1s, CELL 10s, TEMP 5s]",
             activeFaults != 0U ? "ASSERTED" : "CLEAR",
             (unsigned long)activeFaults, activeNames,
             (unsigned long)latchedFaults, latchedNames);
  settleUsbAndServiceCan();
}

static void printPackAndBmsSummary(void)
{
  const float vSense = getTractiveVoltage();
  const float rawCellSum = getPackVoltageFromCells();
  float monitoredCellSum = 0.0f;
  float minCell = 0.0f;
  float maxCell = 0.0f;
  float minTemperature = 0.0f;
  float maxTemperature = 0.0f;
  uint32_t minCellIndex = 0U;
  uint32_t maxCellIndex = 0U;
  uint32_t minTemperatureIndex = 0U;
  uint32_t maxTemperatureIndex = 0U;
  uint32_t monitoredCells = 0U;
  uint32_t readableCells = 0U;
  uint32_t ignoredCells = 0U;
  uint32_t validTemperatures = 0U;
  uint32_t hotTemperatures = 0U;

  for (uint32_t cell = 0U; cell < NUM_BMS_ICS * CELLS_PER_BMB; cell++)
  {
    if (!isCellVoltageMonitored(cell)) {
      ignoredCells++;
      continue;
    }
    monitoredCells++;

    const uint32_t bmb = cell / CELLS_PER_BMB;
    if (!isBmbReadingOk(bmb)) continue;

    const float voltage = getCellVoltage(cell);
    monitoredCellSum += voltage;
    if (readableCells == 0U || voltage < minCell)
    {
      minCell = voltage;
      minCellIndex = cell;
    }
    if (readableCells == 0U || voltage > maxCell)
    {
      maxCell = voltage;
      maxCellIndex = cell;
    }
    readableCells++;
  }

  for (uint32_t temperature = 0U;
       temperature < NUM_BMS_ICS * TEMPERATURES_PER_BMB;
       temperature++)
  {
    if (!isCellTemperatureReadingValid(temperature)) continue;

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
    if (value > 60.0f) hotTemperatures++;
  }

  usb_printf("PACK VSENSE=%.2fV CELL_SUM_RAW=%.2fV DELTA=%.2fV MONITORED_SUM=%.2fV SOC=%.1f%%",
             (double)vSense, (double)rawCellSum,
             (double)(vSense - rawCellSum), (double)monitoredCellSum,
             (double)hvc_can_get_pack_soc());
  settleUsbAndServiceCan();

  if (readableCells > 0U)
  {
    usb_printf("CELLS monitored=%lu readable=%lu ignored=%lu min=C%lu %.3fV max=C%lu %.3fV spread=%.3fV limits=[3.00,4.20]V",
               (unsigned long)monitoredCells,
               (unsigned long)readableCells,
               (unsigned long)ignoredCells,
               (unsigned long)(minCellIndex + 1U), (double)minCell,
               (unsigned long)(maxCellIndex + 1U), (double)maxCell,
               (double)(maxCell - minCell));
  }
  else
  {
    usb_printf("CELLS monitored=%lu readable=0 ignored=%lu min=NONE max=NONE limits=[3.00,4.20]V",
               (unsigned long)monitoredCells,
               (unsigned long)ignoredCells);
  }
  settleUsbAndServiceCan();

  if (validTemperatures > 0U)
  {
    usb_printf("TEMPS valid=%lu/%u invalid=%lu hot=%lu min=T%lu %.1fC max=T%lu %.1fC limit=60.0C",
               (unsigned long)validTemperatures,
               (unsigned int)(NUM_BMS_ICS * TEMPERATURES_PER_BMB),
               (unsigned long)(NUM_BMS_ICS * TEMPERATURES_PER_BMB -
                               validTemperatures),
               (unsigned long)hotTemperatures,
               (unsigned long)(minTemperatureIndex + 1U),
               (double)minTemperature,
               (unsigned long)(maxTemperatureIndex + 1U),
               (double)maxTemperature);
  }
  else
  {
    usb_printf("TEMPS valid=0/%u invalid=%u hot=0 min=NONE max=NONE limit=60.0C",
               (unsigned int)(NUM_BMS_ICS * TEMPERATURES_PER_BMB),
               (unsigned int)(NUM_BMS_ICS * TEMPERATURES_PER_BMB));
  }
  settleUsbAndServiceCan();
}

static void printCellRows(void)
{
  char line[BMS_PRINT_LINE_SIZE];
  usb_printf("CELL READINGS (C14-C17 ignored for BMS faults; ~=ignored, !=fault/comms)");
  settleUsbAndServiceCan();

  for (uint32_t bmb = 0; bmb < NUM_BMS_ICS; bmb++)
  {
    const bool communicationsOk = isBmbReadingOk(bmb);
    bool cellsOk = communicationsOk;
    const uint32_t firstCell = bmb * CELLS_PER_BMB;

    for (uint32_t channel = 0; channel < CELLS_PER_BMB; channel++)
    {
      const uint32_t cellIndex = firstCell + channel;
      if (isCellVoltageMonitored(cellIndex)) {
        cellsOk &= isCellVoltageReadingOk(cellIndex);
      }
    }

    const char *cellStatus = !communicationsOk ? "COMMS" :
                             (cellsOk ? "OK" : "FAULT");
    size_t length = appendToLine(line, sizeof(line), 0U,
                                 "BMB%-2lu CELLS %-5s\t",
                                 (unsigned long)(bmb + 1U), cellStatus);
    for (uint32_t channel = 0; channel < CELLS_PER_BMB; channel++)
    {
      const uint32_t cellIndex = firstCell + channel;
      const bool monitored = isCellVoltageMonitored(cellIndex);
      const bool readingOk = communicationsOk &&
                             isCellVoltageReadingOk(cellIndex);
      const char marker = !monitored ? '~' : (readingOk ? ' ' : '!');
      length = appendToLine(line, sizeof(line), length,
                            "C%-3lu=%c%6.3f%s",
                            (unsigned long)(cellIndex + 1U), marker,
                            (double)getCellVoltage(cellIndex),
                            channel + 1U < CELLS_PER_BMB ? "\t" : "");
    }
    println(line);
    settleUsbAndServiceCan();
  }
}

static void printTemperatureRows(void)
{
  char line[BMS_PRINT_LINE_SIZE];
  usb_printf("TEMPERATURE READINGS (!=missing/high-Z/comms, ^=over 60C)");
  settleUsbAndServiceCan();

  for (uint32_t bmb = 0; bmb < NUM_BMS_ICS; bmb++)
  {
    const bool communicationsOk = isBmbReadingOk(bmb);
    bool allValid = communicationsOk;
    bool anyHot = false;
    const uint32_t firstTemperature = bmb * TEMPERATURES_PER_BMB;

    for (uint32_t channel = 0; channel < TEMPERATURES_PER_BMB; channel++)
    {
      const uint32_t temperatureIndex = firstTemperature + channel;
      allValid &= isCellTemperatureReadingValid(temperatureIndex);
      anyHot |= isCellTemperatureReadingValid(temperatureIndex) &&
                getCellTemperature(temperatureIndex) > 60.0f;
    }

    const char *temperatureStatus = !communicationsOk ? "COMMS" :
                                    (anyHot ? "HOT" :
                                     (allValid ? "OK" : "PARTIAL"));
    size_t length = appendToLine(line, sizeof(line), 0U,
                                 "BMB%-2lu TEMPS %-7s\t",
                                 (unsigned long)(bmb + 1U),
                                 temperatureStatus);
    for (uint32_t channel = 0; channel < TEMPERATURES_PER_BMB; channel++)
    {
      const uint32_t temperatureIndex = firstTemperature + channel;
      const bool valid = isCellTemperatureReadingValid(temperatureIndex);
      const bool hot = valid && getCellTemperature(temperatureIndex) > 60.0f;
      const char marker = !valid ? '!' : (hot ? '^' : ' ');
      length = appendToLine(line, sizeof(line), length,
                            "T%-2lu=%c%6.1f%s",
                            (unsigned long)(temperatureIndex + 1U), marker,
                            (double)getCellTemperature(temperatureIndex),
                            channel + 1U < TEMPERATURES_PER_BMB ? "\t" : "");
    }
    println(line);
    settleUsbAndServiceCan();
  }
}

static void printBmsReadings(void)
{
  hvc_can_rx_status_t canRx;
  hvc_can_get_rx_status(&canRx);

  usb_printf("HVC state=%s SOC=%.1f%% shutdown=%s AIR=[+%u,-%u] VCU_RX=%s age=%lums PRNDL=%u",
             get_state_name(get_current_state()),
             (double)hvc_can_get_pack_soc(),
             isShutdownClosed() ? "CLOSED" : "OPEN",
             (unsigned int)isPosContactorClosed(),
             (unsigned int)isNegContactorClosed(),
             canRx.vcuStateValid ? "OK" : "TIMEOUT",
             (unsigned long)canRx.vcuStateAgeMs,
             (unsigned int)canRx.prndlState);
  settleUsbAndServiceCan();

  usb_printf("SAFETY SHDN=[%u,%u,%u,%u] SHDN12_24V=%s IMD=%s BMB_COMMS=%lu/%u",
             (unsigned int)isShutdownOneOk(),
             (unsigned int)isShutdownTwoOk(),
             (unsigned int)isShutdownThreeOk(),
             (unsigned int)isShutdownFourOk(),
             isShutdownTwelveOk() ? "ON" : "OFF",
             isImdOverrideActive() ? "OVERRIDDEN_LOW" :
                                     (isImdOk() ? "OK" : "FAULT"),
             (unsigned long)getNumResponsiveChips(),
             (unsigned int)NUM_BMS_ICS);
  settleUsbAndServiceCan();

  usb_printf("BALANCE PWM active=%u cells=%lu (muted for C-ADC scans)",
             (unsigned int)isBalancingActive(),
             (unsigned long)getBalanceCount());
  settleUsbAndServiceCan();

  printPackAndBmsSummary();
  printBmsFaultDiagnostics();
  printCellRows();
  printTemperatureRows();
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

    hvc_can_rx_status_t canRx;
    hvc_can_get_rx_status(&canRx);

    /* Temporary PCB bring-up override keeps the IMD relay-hold driven low. */
    imd_can_periodic(canRx.vcuStateValid, canRx.prndlState);

    /* Service safety-critical CAN immediately before the one blocking ADBMS
       conversion step as well as after it in hvc_can_periodic(). */
    hvc_can_service_tx();
    cells_periodic((int)get_current_state());

    if ((uint32_t)(currentTime - lastStateMachineTime) >= 100U)
    {
      lastStateMachineTime = currentTime;
      currentFaults = get_faults();
      latch_faults(currentFaults);
      setBmsError(currentFaults != 0U);

      const bool startupComplete = currentTime > 8000U;
      if (startupComplete)
      {
        bmsIndicatorError = bmsIndicatorError || currentFaults != 0U;
        imdIndicatorError = imdIndicatorError || !isImdOk();
      }

      const bool anyFaults = get_latched_faults() != 0U || !startupComplete;
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
