/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "lptim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ssd1306.h"
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

void drawBitmap32x64(int x, int y, const uint8_t *bitmap) {
  for (int col = 0; col < 32; col++) {
    for (int byteRow = 0; byteRow < 8; byteRow++) {
      uint8_t byte = bitmap[col * 8 + byteRow];
      for (int bit = 0; bit < 8; bit++) {
        if (byte & (1 << bit)) {
					ssd1306_DrawPixel(x + col, y + byteRow * 8 + bit, White);
        }
      }
    }
  }
}

void DrawChar(char ch, int x_, int y_, const uint8_t *bitmap)
{
	uint8_t w = bitmap[0];
	uint8_t h = bitmap[1];
	uint8_t hB = h/8;
	uint8_t first_ch_ascii = bitmap[2];
	const uint8_t* dig = bitmap + 4 + (w * hB * (ch-first_ch_ascii));
	for (int x = 0; x < w; x++)
	{
		for (int y = 0; y < hB; y++)
		{
			//DrawPageSeg(x, y, *(dig++))	;
		}
		//ssd1306_UpdateScreen();
	}
	
}

void fill(int y_)
{
	for (int y = 0; y < SSD1306_HEIGHT; y++)
	{
		for (int x = 0; x < SSD1306_WIDTH; x++)
		{
			if((x&1)&&(y&1)) 
				ssd1306_DrawPixel(x, y, White);
			else 
				ssd1306_DrawPixel(x, y, Black);
		}
	}
	
}
extern void tmp(void);
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
  MX_ADC_Init();
  MX_I2C1_Init();
  MX_LPTIM1_Init();
  /* USER CODE BEGIN 2 */
  ssd1306_Init();
	
	
	
	tmp();
	
	extern const SSD1306_Font_t Font_6x8;
	extern const SSD1306_Font_t Font_16x26;
	ssd1306_SetCursor(0,0);
	ssd1306_WriteString("1234567890", Font_6x8, White);
	ssd1306_UpdateScreen();
	
	/*ssd1306_SetCursor(0,0);
	ssd1306_WriteString("1234567890", Font_16x26, White);
	ssd1306_UpdateScreen();*/
	
	return 0;
	for(int i=0; i<255; i++)
	{
		//DrawPageSeg(0, 0, i);
		ssd1306_UpdateScreen();
	}
	//ssd1306_WriteString("7885", Font_16x24, White);
	ssd1306_Fill(White);
//	DrawChar('8', 0,  0, SmallFont);
	ssd1306_UpdateScreen();
	
	for(int i=0; i<SSD1306_HEIGHT; i++)
	{
		//fill(i);
		//ssd1306_UpdateScreen();
	}
	
	//drawBitmap32x64(0,0,font_32x64_digit_8);
	/*extern uint8_t SSD1306_Buffer[SSD1306_BUFFER_SIZE];
	for(int i=0; i<SSD1306_BUFFER_SIZE; i++) SSD1306_Buffer[i] = 0;
	for(int i=0; i<SSD1306_BUFFER_SIZE; i++)
	{
		SSD1306_Buffer[i]=0xFF;
		ssd1306_UpdateScreen();
	}*/
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLLMUL_4;
  RCC_OscInitStruct.PLL.PLLDIV = RCC_PLLDIV_2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C1|RCC_PERIPHCLK_LPTIM1;
  PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_PCLK1;
  PeriphClkInit.LptimClockSelection = RCC_LPTIM1CLKSOURCE_PCLK;

  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
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
