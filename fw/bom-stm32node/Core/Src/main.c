/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include "i2c.h"
#include "sai.h"
#include "spi.h"
#include "usart.h"
#include "usb.h"
#include "app_usbx.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usb_cli.h"
#include "radio.h"
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
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_SPI1_Init();
  MX_USB_PCD_Init();
  MX_I2C2_Init();
  MX_SAI1_Init();
  MX_SPI3_Init();
  MX_ADC1_Init();
  MX_ADC2_Init();
  MX_USART1_UART_Init();
  MX_UART4_Init();
  MX_SPI5_Init();
  MX_USBX_Init();
  /* USER CODE BEGIN 2 */
  /* The microphone (SAI1 + GPDMA, per docs/pdm-port-plan.md "CubeMX contract")
     is started on demand by the `stream` command (see pcm_stream.c), so there
     is nothing to start here. */

  /* SX1262 bring-up (SPI1 + EN_LORA/NRST/BUSY/DIO1, see App/radio/radio.h),
     BEFORE the USB pull-up goes live (usb_cli_start(), next): RadioLib's
     internal chip-detect retries the reset/SPI probe up to 10 times with a
     1 s standby-verify timeout each when no module answers, so radio_init()
     can block for several seconds. Doing that before usb_cli_start() means
     the host never sees the device on the bus mid-enumeration during that
     window - USB simply appears a few seconds later instead of an
     enumeration attempt timing out because nothing was servicing the USB
     stack. A radio failure itself is never fatal: the radio stays disabled
     and `radio status` over the CLI reports why. */
  (void)radio_init();

  /* USB CDC command console (endpoint PMA + PCD start + CLI, see usb_cli.c).
     BEFORE the instruction cache is enabled below: usb_cli_start() ->
     cli_init() -> link_service_init() reads the chip's factory UID
     (HAL_GetUIDw0()/w1()/w2(), App/link/link_service.c's derive_node_id()/
     derive_session_id() and App/link/boomlink_radio_port.c's PRNG seed -
     the only three call sites of those HAL functions anywhere in this
     firmware, all reached exactly once here, synchronously, and never
     again afterwards) from UID_BASE (0x08FFF800 - the STM32H563's OTP-style
     Flash-size/UID/package info block, not the flash array proper). Found
     by hardware bring-up: with the cache enabled first (as this file used
     to do), that read HardFaults - a precise BusFault at BFAR == UID_BASE,
     confirmed identical on two independently flashed boards, confirmed the
     address itself is valid and its data plausible by reading it directly
     over SWD (which bypasses the CPU and its cache entirely and succeeds).
     The CPU's own cached read is what faults, not the address - a burst
     line-fill transaction this info block's controller does not answer the
     same way a debug probe's single AHB-AP read does. Deferring the enable
     until after these three one-time reads sidesteps the mechanism
     entirely, whatever ST's cache implementation is actually doing
     underneath, without needing to disable/re-enable the cache around a
     read that never recurs. */
  usb_cli_start();

  /* Enable the instruction cache. The core runs code from flash at 250 MHz with
     5 wait states (FLASH_LATENCY_5); with the cache off, every instruction fetch
     stalls the CPU and the PDM->PCM DSP misses its 21.33 ms/ring-half real-time
     budget (~39 ms measured -> mic overrun). Cached it takes ~17 ms and fits.
     CubeMX leaves HAL_ICACHE_MODULE_ENABLED off in stm32h5xx_hal_conf.h, so the
     cache is driven directly here (keeps the CubeMX-owned conf untouched). The
     cache is invalidated on reset; wait for any pending invalidation, then
     enable it for the whole firmware.
     Deliberately AFTER usb_cli_start() (see that call's own comment for why -
     the UID-reading HardFault this ordering avoids), not before: the mic
     streaming path this cache exists for is only ever started later, on
     demand, by the `stream`/`streamtest` CLI commands the user issues once
     the board is already up and enumerated - comfortably after this point -
     so moving the enable this much later costs nothing the DSP budget above
     actually needs. */
  while ((ICACHE->SR & ICACHE_SR_BUSYF) != 0u)
  {
  }
  ICACHE->CR |= ICACHE_CR_EN;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* Drain the DIO1 event flag and drive RadioLib's TX/RX completion
       handling BEFORE usb_cli_process(): a `radio ping` dispatched from the
       CLI calls radio_send(), which changes the radio's mode. Servicing any
       event already pending from *before* this iteration first means a
       just-arrived RX-done can never be misread as the completion of a
       transmission the CLI is about to start in this same iteration (radio_
       send() also flushes defensively on its own - see radio.cpp - but
       draining here first is what keeps that from being load-bearing).
       Never runs from interrupt context - see boomlink.md section 6.2. Not
       serviced while a `stream`/`streamtest` command blocks the loop
       (documented limitation, same section). */
    radio_process();

    /* Service USB: enumeration, the CDC console, and PCM streaming. The
       `stream`/`streamtest` commands start the microphone on demand and run the
       whole transfer synchronously from here, so this loop has no separate
       mic_poll drain (a second consumer would steal ring halves). */
    usb_cli_process();
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
  RCC_CRSInitTypeDef RCC_CRSInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48|RCC_OSCILLATORTYPE_HSE
                              |RCC_OSCILLATORTYPE_CSI;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.CSIState = RCC_CSI_ON;
  RCC_OscInitStruct.CSICalibrationValue = RCC_CSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLL1_SOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 2;
  RCC_OscInitStruct.PLL.PLLN = 40;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1_VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1_VCORANGE_WIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_PCLK3;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enable the CRS APB clock
  */
  __HAL_RCC_CRS_CLK_ENABLE();

  /** Configures CRS
  */
  RCC_CRSInitStruct.Prescaler = RCC_CRS_SYNC_DIV1;
  RCC_CRSInitStruct.Source = RCC_CRS_SYNC_SOURCE_USB;
  RCC_CRSInitStruct.Polarity = RCC_CRS_SYNC_POLARITY_RISING;
  RCC_CRSInitStruct.ReloadValue = __HAL_RCC_CRS_RELOADVALUE_CALCULATE(48000000,1000);
  RCC_CRSInitStruct.ErrorLimitValue = 34;
  RCC_CRSInitStruct.HSI48CalibrationValue = 32;

  HAL_RCCEx_CRSConfig(&RCC_CRSInitStruct);

  /** Configure the programming delay
  */
  __HAL_FLASH_SET_PROGRAM_DELAY(FLASH_PROGRAMMING_DELAY_2);
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @param None
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
#ifdef USE_FULL_ASSERT
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
