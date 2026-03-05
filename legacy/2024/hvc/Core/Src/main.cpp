/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
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
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "imd.h"
#include "state_machine.h"
#include "vcu.h"
#include "charging.h"
#include "vsense.h"
#include "isense.h"
#include "tsense.h"
#include "cells.h"
#include "LonghornLib/clock.h"
#include "fans.h"
#include "LonghornLib/led.h"
#include "usb.h"
#include "LonghornLib/imu.h"
#include "contactors.h"
#include "LonghornLib/angel_can.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

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

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static bool hvOk = true;
static bool hvOkEver = false;
static float hvFaultTimer = 0;
static int state = STATE_NOT_ENERGIZED;
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
  MX_FDCAN1_Init();
  MX_ADC1_Init();
  MX_SPI1_Init();
  MX_TIM3_Init();
  MX_SPI2_Init();
  MX_TIM5_Init();
  MX_UART4_Init();
  MX_FDCAN2_Init();
  /* USER CODE BEGIN 2 */
  clock_init();
  led_init();
  can_init(&hfdcan1);

//  tsenseInit();
  chargingInit();
  fansInit();

  vcuInit();
  stateMachineInit();
  cellsInit();


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    float deltaTime = clock_getDeltaTime();

    volatile bool imdOk = isImdOk();
    volatile bool shutdownClosed = isShutdownClosed() && (clock_getTime() > 1.0f);
    volatile bool chargerPresent = isChargerPluggedIn();

    volatile bool hvOkInstant;

#ifdef PRINT_BMS_DATA
    static int cc = 0;
    cc++;
    bool timeToPrint = (((cc-1) % (270*10)) == 0) && (state != STATE_ENERGIZED);

    if(timeToPrint)
    {
      std::string space, s;

      s = "isospi responsive?";
      println(space);
      println(s);
      println(isIsoSpiResponsive());

      s = "cell voltages ok?";
      println(space);
      println(s);
      println(areCellVoltagesWithinBounds());

      s = "cell temps ok?";
      println(space);
      println(s);
      println(isTempWithinBounds());

      s = "pack voltage ok?";
      println(space);
      println(s);
      println(isPackVoltageWithinBounds());

      s = "pack current ok?";
      println(space);
      println(s);
      println(isPackCurrentWithinBounds());

      s = "imd ok?";
      println(space);
      println(s);
      println(imdOk);

      s = "shutdown closed?";
      println(space);
      println(s);
      println(shutdownClosed);

      s = "charger present?";
      println(space);
      println(s);
      println(chargerPresent);

      s = "voltage";
      std::string voltageMessage = std::to_string(getTractiveVoltage()) + " / " + std::to_string(getPackVoltageFromCells());
      println(space);
      println(s);
      println(voltageMessage);

      println(space);
    }
#endif

    hvOkInstant = isIsoSpiResponsive();
    hvOkInstant = hvOkInstant && isTempWithinBounds();
    hvOkInstant = hvOkInstant && isPackVoltageWithinBounds();
    hvOkInstant = hvOkInstant && isPackCurrentWithinBounds();
    hvOkInstant = hvOkInstant && areCellVoltagesWithinBounds();
    hvOkEver = hvOkEver || hvOkInstant;

    if (hvOkInstant) {
      hvOk = true;
      hvFaultTimer = 0;
    } else {
      hvFaultTimer += deltaTime;
      if (hvFaultTimer > 5.0f) {
        hvOk = false;
      }
    }

    bool amsError = !(hvOk && hvOkEver);
    bool imdError = !imdOk;
    hvOk = hvOk && hvOkEver && imdOk;

    if (clock_getTime() > 6.0f) {
      writeAmsError(amsError);
    }

    state = updateStateMachine(shutdownClosed, hvOk && hvOkEver, chargerPresent, deltaTime);

    volatile float vPack = getPackVoltageFromCells();
    volatile float vSense = getTractiveVoltage();

    // println(vSense);

    cellsPeriodic(state);
    vcuPeriodic(amsError, imdError, state, deltaTime);

    float maxCellMargin = getMaxCellMargin();
    chargingPeriodic(shutdownClosed, state == STATE_CHARGING, amsError, imdError, vPack, maxCellMargin, deltaTime);
    fansPeriodic(deltaTime);
    can_periodic(deltaTime);
    //    tsensePeriodic();

//    uint32_t ics = getNumResponsiveChips();
//    if(ics == 0) {
//      led_set(0.3f, 0, 0);
//    } else if(ics % 2 == 1) {
//      led_set(0.3f, 0.3f, 0);
//    } else if(ics == 2) {
//      led_set(0.3f, 0, 0.3f);
//    } else if(ics == 4) {
//      led_set(0, 0, 0.3f);
//    } else if(ics == 6) {
//      led_set(0, 0.3f, 0.3f);
//    } else if(ics == 8) {
//      led_set(0, 0.3f, 0);
//    } else if(ics == 10) {
//      led_set(0.3f, 0.3f, 0.3f);
//    }
//    println(ics);

    if (hvOk && hvOkEver) {
      if (state == STATE_PRECHARGING) {
        led_set(0.25f, 0.25f, 0);
      } else if (state == STATE_ENERGIZED) {
        led_set(0, 0.4f, 0);
      } else if (state == STATE_CHARGING_PRECHARGING) {
        led_set(0, 0.25f, 0.25f);
      } else if (state == STATE_CHARGING) {
        led_set(0, 0, 0.4f);
      } else { // shutdown circuit open
        led_set(0.3f, 0.3f, 0.3f);
      }
    } else {
      led_set(0.3f, 0, 0);
    }

    HAL_GPIO_WritePin(TSV_GPIO_Port, TSV_Pin,
                      (getTractiveVoltage() > 75.0f) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    HAL_Delay(1);
  }
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

  /*AXI clock gating */
  RCC->CKGAENR = 0xFFFFFFFF;

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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_DIV4;
  RCC_OscInitStruct.HSICalibrationValue = 64;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 35;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 35;
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
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_6) != HAL_OK)
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
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SPI2|RCC_PERIPHCLK_SPI1;
  PeriphClkInitStruct.PLL3.PLL3M = 1;
  PeriphClkInitStruct.PLL3.PLL3N = 8;
  PeriphClkInitStruct.PLL3.PLL3P = 2;
  PeriphClkInitStruct.PLL3.PLL3Q = 2;
  PeriphClkInitStruct.PLL3.PLL3R = 2;
  PeriphClkInitStruct.PLL3.PLL3RGE = RCC_PLL3VCIRANGE_3;
  PeriphClkInitStruct.PLL3.PLL3VCOSEL = RCC_PLL3VCOWIDE;
  PeriphClkInitStruct.PLL3.PLL3FRACN = 0;
  PeriphClkInitStruct.Spi123ClockSelection = RCC_SPI123CLKSOURCE_PLL3;
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
  while (1) {
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
