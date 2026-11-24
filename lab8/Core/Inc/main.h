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
#include "stm32f7xx_hal.h"

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
#define t_irq_Pin GPIO_PIN_2
#define t_irq_GPIO_Port GPIOE
#define t_clk_Pin GPIO_PIN_3
#define t_clk_GPIO_Port GPIOE
#define t_miso_Pin GPIO_PIN_4
#define t_miso_GPIO_Port GPIOE
#define t_mosi_Pin GPIO_PIN_5
#define t_mosi_GPIO_Port GPIOE
#define t_cs_Pin GPIO_PIN_6
#define t_cs_GPIO_Port GPIOE
#define userblue_Pin GPIO_PIN_13
#define userblue_GPIO_Port GPIOC
#define ld3_Pin GPIO_PIN_14
#define ld3_GPIO_Port GPIOB
#define usbpower_Pin GPIO_PIN_6
#define usbpower_GPIO_Port GPIOG
#define usbover_Pin GPIO_PIN_7
#define usbover_GPIO_Port GPIOG
#define cs_Pin GPIO_PIN_8
#define cs_GPIO_Port GPIOC
#define dc_Pin GPIO_PIN_9
#define dc_GPIO_Port GPIOC
#define rst_Pin GPIO_PIN_10
#define rst_GPIO_Port GPIOC
#define ld2_Pin GPIO_PIN_7
#define ld2_GPIO_Port GPIOB
#define i2c1_scl_Pin GPIO_PIN_8
#define i2c1_scl_GPIO_Port GPIOB
#define i2c1_sda_Pin GPIO_PIN_9
#define i2c1_sda_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
