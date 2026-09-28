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
#include "usb_vcp.h"
#include "vct_sense.h"
#include "state_machine.h"
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

static void printBmsReadings(void)
{
  char line[BMS_PRINT_LINE_SIZE];

  usb_printf("HVC state=%s shutdown=%u AIR+=%u AIR-=%u faults=0x%02lX latched=0x%02lX",
             get_state_name(get_current_state()),
             (unsigned int)isNegContactorClosed(),
             (unsigned int)isPosContactorClosed(),
             (unsigned int)isNegContactorClosed(),
             (unsigned long)get_last_faults(),
             (unsigned long)get_latched_faults());
  HAL_Delay(USB_PRINT_SETTLE_MS);

  usb_printf("ADBMS scan: %lu/%u BMBs OK | ! = CRC failure or out-of-range reading",
             (unsigned long)getNumResponsiveChips(), (unsigned int)NUM_BMS_ICS);
  HAL_Delay(USB_PRINT_SETTLE_MS);

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
    HAL_Delay(USB_PRINT_SETTLE_MS);

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
    HAL_Delay(USB_PRINT_SETTLE_MS);
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
  RCC_OscInitStruct.PLL.PLLN = 44;
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
  PeriphClkInitStruct.PLL2.PLL2N = 16;
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
