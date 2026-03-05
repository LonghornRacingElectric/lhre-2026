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
#include "fatfs.h"
#include "fdcan.h"
#include "usart.h"
#include "sdmmc.h"
#include "spi.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "LonghornLib/clock.h"
#include "LonghornLib/led.h"
#include "inverter.h"
#include "LonghornLib/faults.h"
#include "gps.h"
#include "LonghornLib/angel_can.h"
#include "cellular.h"
#include "pdu.h"
#include "hvc.h"
#include "dash.h"
#include "drive_switch.h"
#include "indicators.h"
#include "all_imus.h"
#include "wheelspeeds.h"
#include "nvm.h"
#include "vcu.h"
#include "usb.h"
#include "bevo.h"

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
VcuParameters vcuCoreParameters;
VcuOutput vcuCoreOutput;
AnalogVoltages analogVoltages;
DriveSwitchState driveSwitchState;
HvcStatus hvcStatus;
PduStatus pduStatus;
InverterStatus inverterStatus;
WheelMagnetValues wheelMagnetValues;
ImuData imuData;
GpsData gpsData;

bool didWrite = false;

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
  MX_UART7_Init();
  MX_TIM2_Init();
  MX_ADC1_Init();
  MX_FATFS_Init();
  MX_FDCAN2_Init();
  MX_SDMMC1_SD_Init();
  MX_TIM5_Init();
  MX_USART1_UART_Init();
  MX_UART4_Init();
  MX_LPUART1_UART_Init();
  MX_SPI2_Init();
  /* USER CODE BEGIN 2 */
  led_init();
  clock_init();
  can_init(&hfdcan2);

  vcu_init(vcuCoreParameters);
  inverter_init();
  dash_init();
  hvc_init();
  pdu_init();
  wheelspeeds_init();
  allImus_init(&hspi2);
  gps_init();
  indicators_init();
  // cellular_init();
  nvm_init(&vcuCoreParameters);
  bevo_init(); //

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    float deltaTime = clock_getDeltaTime();
    if(didWrite) led_rainbow(deltaTime);

    adc_periodic(&analogVoltages);
    driveSwitch_periodic(&driveSwitchState);
    hvc_periodic(&hvcStatus, &vcuCoreOutput);
    pdu_periodic(&pduStatus, &vcuCoreOutput);
    wheelspeeds_periodic(&wheelMagnetValues);
    allImus_periodic(&imuData);
    gps_periodic(&gpsData);
    bevo_send(&gpsData);

    // TODO why is front right not working??
    wheelMagnetValues.fr = wheelMagnetValues.fl;

    // println(analogVoltages.apps1);
    // println(analogVoltages.apps2);
    // println(vcuCoreOutput.telemetryApps1);
    // println(vcuCoreOutput.telemetryApps2);

    vcu_execute(analogVoltages, driveSwitchState, hvcStatus, pduStatus, inverterStatus,
                wheelMagnetValues, imuData, gpsData, vcuCoreOutput, deltaTime);


    inverter_periodic(&inverterStatus, &vcuCoreOutput, deltaTime);
    indicators_periodic(&hvcStatus, &vcuCoreOutput);
    dash_periodic(&pduStatus, &hvcStatus, &inverterStatus, &gpsData, &vcuCoreOutput);
    can_periodic(deltaTime);

    // println(cellular_debugMessage);

    nvm_periodic(&vcuCoreParameters, &vcuCoreOutput, &hvcStatus,
                 &pduStatus, &inverterStatus, &analogVoltages,
                 &wheelMagnetValues, &imuData, &gpsData);
    // cellular_periodic(&vcuCoreParameters, &vcuCoreOutput, &hvcStatus,
    //                   &pduStatus, &inverterStatus, &analogVoltages,
    //                   &wheelMagnetValues, &imuData, &gpsData);
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 35;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 35;
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
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_ADC|RCC_PERIPHCLK_SPI2
                              |RCC_PERIPHCLK_UART7|RCC_PERIPHCLK_UART4
                              |RCC_PERIPHCLK_LPUART1;
  PeriphClkInitStruct.PLL2.PLL2M = 10;
  PeriphClkInitStruct.PLL2.PLL2N = 288;
  PeriphClkInitStruct.PLL2.PLL2P = 125;
  PeriphClkInitStruct.PLL2.PLL2Q = 125;
  PeriphClkInitStruct.PLL2.PLL2R = 2;
  PeriphClkInitStruct.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_0;
  PeriphClkInitStruct.PLL2.PLL2VCOSEL = RCC_PLL2VCOMEDIUM;
  PeriphClkInitStruct.PLL2.PLL2FRACN = 0;
  PeriphClkInitStruct.PLL3.PLL3M = 1;
  PeriphClkInitStruct.PLL3.PLL3N = 8;
  PeriphClkInitStruct.PLL3.PLL3P = 2;
  PeriphClkInitStruct.PLL3.PLL3Q = 2;
  PeriphClkInitStruct.PLL3.PLL3R = 4;
  PeriphClkInitStruct.PLL3.PLL3RGE = RCC_PLL3VCIRANGE_3;
  PeriphClkInitStruct.PLL3.PLL3VCOSEL = RCC_PLL3VCOWIDE;
  PeriphClkInitStruct.PLL3.PLL3FRACN = 0;
  PeriphClkInitStruct.Spi123ClockSelection = RCC_SPI123CLKSOURCE_PLL3;
  PeriphClkInitStruct.Usart234578ClockSelection = RCC_USART234578CLKSOURCE_PLL2;
  PeriphClkInitStruct.Lpuart1ClockSelection = RCC_LPUART1CLKSOURCE_PLL2;
  PeriphClkInitStruct.AdcClockSelection = RCC_ADCCLKSOURCE_PLL3;
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
    led_set(0.2f, 0.0f, 0.0f);
    for (volatile int i = 0; i < 10000000; i++); // faster for some reason?
    led_off();
    for (volatile int j = 0; j < 3000000; j++);
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
