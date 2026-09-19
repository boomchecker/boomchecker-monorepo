/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h5xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define PDM_CLK_Pin GPIO_PIN_2
#define PDM_CLK_GPIO_Port GPIOE
#define PDM_D2_Pin GPIO_PIN_4
#define PDM_D2_GPIO_Port GPIOE
#define PDM_D1_Pin GPIO_PIN_6
#define PDM_D1_GPIO_Port GPIOE
#define MAG_SDA_Pin GPIO_PIN_0
#define MAG_SDA_GPIO_Port GPIOF
#define MAG_SCL_Pin GPIO_PIN_1
#define MAG_SCL_GPIO_Port GPIOF
#define SD_DET_Pin GPIO_PIN_2
#define SD_DET_GPIO_Port GPIOF
#define SD_CS_Pin GPIO_PIN_3
#define SD_CS_GPIO_Port GPIOF
#define SD_SCK_Pin GPIO_PIN_7
#define SD_SCK_GPIO_Port GPIOF
#define SD_MISO_Pin GPIO_PIN_8
#define SD_MISO_GPIO_Port GPIOF
#define SD_MOSI_Pin GPIO_PIN_9
#define SD_MOSI_GPIO_Port GPIOF
#define PDM_D3_Pin GPIO_PIN_10
#define PDM_D3_GPIO_Port GPIOF
#define MIC1_O_Pin GPIO_PIN_0
#define MIC1_O_GPIO_Port GPIOA
#define MIC2_O_Pin GPIO_PIN_1
#define MIC2_O_GPIO_Port GPIOA
#define LORA_NSS_Pin GPIO_PIN_4
#define LORA_NSS_GPIO_Port GPIOA
#define LORA_SCK_Pin GPIO_PIN_5
#define LORA_SCK_GPIO_Port GPIOA
#define LORA_MISO_Pin GPIO_PIN_6
#define LORA_MISO_GPIO_Port GPIOA
#define LORA_MOSI_Pin GPIO_PIN_7
#define LORA_MOSI_GPIO_Port GPIOA
#define LORA_NRST_Pin GPIO_PIN_4
#define LORA_NRST_GPIO_Port GPIOC
#define LORA_RXEN_Pin GPIO_PIN_5
#define LORA_RXEN_GPIO_Port GPIOC
#define LORA_TXEN_Pin GPIO_PIN_0
#define LORA_TXEN_GPIO_Port GPIOB
#define LORA_BUSY_Pin GPIO_PIN_1
#define LORA_BUSY_GPIO_Port GPIOB
#define LORA_DIO1_Pin GPIO_PIN_2
#define LORA_DIO1_GPIO_Port GPIOB
#define LORA_DIO2_Pin GPIO_PIN_11
#define LORA_DIO2_GPIO_Port GPIOF
#define EN_LORA_Pin GPIO_PIN_7
#define EN_LORA_GPIO_Port GPIOE
#define VCP_RX_Pin GPIO_PIN_14
#define VCP_RX_GPIO_Port GPIOB
#define VCP_TX_Pin GPIO_PIN_15
#define VCP_TX_GPIO_Port GPIOB
#define IMU_SCK_Pin GPIO_PIN_10
#define IMU_SCK_GPIO_Port GPIOC
#define IMU_MISO_Pin GPIO_PIN_11
#define IMU_MISO_GPIO_Port GPIOC
#define IMU_MOSI_Pin GPIO_PIN_12
#define IMU_MOSI_GPIO_Port GPIOC
#define IMU_CS_Pin GPIO_PIN_0
#define IMU_CS_GPIO_Port GPIOD
#define IMU_INT1_Pin GPIO_PIN_1
#define IMU_INT1_GPIO_Port GPIOD
#define IMU_INT2_Pin GPIO_PIN_3
#define IMU_INT2_GPIO_Port GPIOD
#define IMU_EN_Pin GPIO_PIN_14
#define IMU_EN_GPIO_Port GPIOG
#define GPS_1PPS_Pin GPIO_PIN_15
#define GPS_1PPS_GPIO_Port GPIOG
#define GPS_WAKE_UP_Pin GPIO_PIN_4
#define GPS_WAKE_UP_GPIO_Port GPIOB
#define GPS_RESET_Pin GPIO_PIN_5
#define GPS_RESET_GPIO_Port GPIOB
#define GPS_SCL_Pin GPIO_PIN_6
#define GPS_SCL_GPIO_Port GPIOB
#define GPS_SDA_Pin GPIO_PIN_7
#define GPS_SDA_GPIO_Port GPIOB
#define GPS_TX_Pin GPIO_PIN_8
#define GPS_TX_GPIO_Port GPIOB
#define GPS_RX_Pin GPIO_PIN_9
#define GPS_RX_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
