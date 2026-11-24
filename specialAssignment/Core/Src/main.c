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
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum
{
	ST_ASK_NAME = 0,
	ST_WAIT_PEER_NAME,
	ST_MY_TURN,
	ST_PEER_TURN,
	ST_DONE
} t_chat_state;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define LINE_BUF_SIZE	128U
#define	NAME_BUF_SIZE	32U

#define IS_INIT	0
//#define IS_INIT	1
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
//UART3 (Terminal)
static uint8_t					termRxByte;
static char						termLineBuf[LINE_BUF_SIZE];
static volatile uint16_t		termIdx = 0;
static volatile uint8_t			termLineReady = 0;

//UART4 (Link between MCUs)
static uint8_t					linkRxByte;
static char						linkLineBuf[LINE_BUF_SIZE];
static volatile uint16_t		linkIdx = 0;
static volatile uint8_t			linkLineReady = 0;

static char						myName[NAME_BUF_SIZE]   = {0};
static char						peerName[NAME_BUF_SIZE] = {0};

static volatile t_chat_state	chatState = ST_ASK_NAME;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */
static void	ChatPrint(const char *s);
static void ChatPrintPrompt(void);
static void EndChat(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
//Helper for blocking transmit on UART3
static void ChatPrint(const char *s)
{
	HAL_UART_Transmit(&huart3, (uint8_t *)s, (uint16_t)strlen(s), HAL_MAX_DELAY);
}

//Print {myName} =>
static void ChatPrintPrompt(void)
{
  ChatPrint(myName);
  ChatPrint(" => ");
}

//Stop UART reception and notify
static void EndChat(void)
{
  HAL_UART_AbortReceive_IT(&huart3);
  HAL_UART_AbortReceive_IT(&huart4);
  ChatPrint("\r\n*** Communication is over!! ***\r\n");
  chatState = ST_DONE;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart->Instance == USART3)
	{
		// no buffer
		if (chatState != ST_ASK_NAME && chatState != ST_MY_TURN)
		{
			termIdx = 0;
			termLineReady = 0;
			HAL_UART_Receive_IT(&huart3, &termRxByte, 1);
			return;
		}

		if (termRxByte == '\r' || termRxByte == '\n')
		{
			if (termIdx > 0)
			{
				termLineBuf[termIdx] = '\0';
				termIdx = 0;
				termLineReady = 1;
			}
			ChatPrint("\r\n");
		}
		else if (termRxByte == 0x08 || termRxByte == 0x7F) /* backspace */
		{
			if (termIdx > 0)
			{
				termIdx--;
				ChatPrint("\b \b");
			}
		}
		else
		{
			if (termIdx < (LINE_BUF_SIZE - 1))
			{
				termLineBuf[termIdx++] = (char)termRxByte;
				HAL_UART_Transmit(&huart3, &termRxByte, 1, HAL_MAX_DELAY); /* local echo */
			}
		}
		HAL_UART_Receive_IT(&huart3, &termRxByte, 1);
	}
	else if (huart->Instance == UART4)
	{
		// no buffer
		if (chatState != ST_WAIT_PEER_NAME && chatState != ST_PEER_TURN)
		{
			linkIdx = 0;
			linkLineReady = 0;
			HAL_UART_Receive_IT(&huart4, &linkRxByte, 1);
			return;
		}

		if (linkRxByte == '\r' || linkRxByte == '\n')
		{
			if (linkIdx > 0)
			{
				linkLineBuf[linkIdx] = '\0';
				linkIdx = 0;
				linkLineReady = 1;
			}
		}
		else
		{
			if (linkIdx < (LINE_BUF_SIZE - 1))
			{
				linkLineBuf[linkIdx++] = (char)linkRxByte;
			}
		}
		HAL_UART_Receive_IT(&huart4, &linkRxByte, 1);
	}
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
	if (huart->Instance == USART3)
	{
		__HAL_UART_CLEAR_OREFLAG(huart);
		HAL_UART_Receive_IT(&huart3, &termRxByte, 1);
	}
	else if (huart->Instance == UART4)
	{
		__HAL_UART_CLEAR_OREFLAG(huart);
		HAL_UART_Receive_IT(&huart4, &linkRxByte, 1);
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

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

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
  MX_USART3_UART_Init();
  MX_UART4_Init();
  /* USER CODE BEGIN 2 */
  HAL_Delay(100); // รอจังหวะการเชื่อมต่อ Serial Port บน PC ให้พร้อม

#if IS_INIT
  ChatPrint("Man from U.A.R.T.1!\r\n");
  ChatPrint("Quit PRESS q\r\n");
  ChatPrint("\tName: ");
  chatState = ST_ASK_NAME;
#else
  ChatPrint("Man from U.A.R.T.2!\r\n");
  ChatPrint("Quit PRESS q\r\n");
  chatState = ST_WAIT_PEER_NAME;
#endif

  HAL_UART_Receive_IT(&huart3, &termRxByte, 1);
  HAL_UART_Receive_IT(&huart4, &linkRxByte, 1);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	switch(chatState)
	{
		case ST_ASK_NAME:
			if (termLineReady)
			{
				termLineReady = 0;
				strncpy(myName, termLineBuf, NAME_BUF_SIZE - 1);
				myName[NAME_BUF_SIZE - 1] = '\0';

				// Send Name to peer board
				HAL_UART_Transmit(&huart4, (uint8_t *)myName, (uint16_t)strlen(myName), HAL_MAX_DELAY);
				HAL_UART_Transmit(&huart4, (uint8_t *)"\r\n", 2, HAL_MAX_DELAY);

			#if IS_INIT
				chatState = ST_WAIT_PEER_NAME;
			#else
				chatState = ST_PEER_TURN;
			#endif
			}
			break;

		case ST_WAIT_PEER_NAME:
			if (linkLineReady)
			{
				linkLineReady = 0;
				strncpy(peerName, linkLineBuf, NAME_BUF_SIZE - 1);
				peerName[NAME_BUF_SIZE - 1] = '\0';

				ChatPrint(peerName);
				ChatPrint(" is ready\r\n");

			#if IS_INIT
				ChatPrintPrompt();
				chatState = ST_MY_TURN;
			#else
				ChatPrint("\tName: ");
				chatState = ST_ASK_NAME;
			#endif
			}
			break;

		case ST_MY_TURN:
			if (termLineReady)
			{
				termLineReady = 0;

				if (strcmp(termLineBuf, "q") == 0)
				{
					HAL_UART_Transmit(&huart4, (uint8_t *)"q\r\n", 3, HAL_MAX_DELAY);
					EndChat();
				}
				else
				{
					HAL_UART_Transmit(&huart4, (uint8_t *)termLineBuf, (uint16_t)strlen(termLineBuf), HAL_MAX_DELAY);
					HAL_UART_Transmit(&huart4, (uint8_t *)"\r\n", 2, HAL_MAX_DELAY);

					chatState = ST_PEER_TURN;
				}
			}
			break;

		case ST_PEER_TURN:
			if (linkLineReady)
			{
				linkLineReady = 0;
				if (strcmp(linkLineBuf, "q") == 0)
				{
					EndChat();
				}
				else
				{
					ChatPrint(peerName);
					ChatPrint(" : ");
					ChatPrint(linkLineBuf);
					ChatPrint("\r\n");
					ChatPrintPrompt();
					chatState = ST_MY_TURN;
				}
			}
			break;

		case ST_DONE:
		default:
			break;
	}
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 216;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_7) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

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
