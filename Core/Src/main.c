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
#define UPDATE_MS 00
#define ADC_COUNT 16
#define GND_RES 	9980 // Резистор в делителе у земли
#define POW_RES 	46950 // Резистор в делителе у питания 
#define VREF			1224 // mV

#define DS1621_I2C_ADDR	(0x4F<<1)
#define DS1621_CFG		0xAC
#define DS1621_START	0xEE
#define DS1621_READ		0xAA
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint16_t v_ref;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
extern const uint8_t SmallFont[];
extern const uint8_t Medium[];

uint16_t GetMedium(uint16_t* buff, int size)
{
		for (uint32_t i = 0; i < size - 1; i++) 
		{
		uint32_t min_idx = i;
    for (uint32_t j = i + 1; j < size; j++) 
		{
			if (buff[j] < buff[min_idx])
					min_idx = j;
    }
    uint16_t temp = buff[min_idx];
    buff[min_idx] = buff[i];
    buff[i] = temp;
	}	
	return buff[size >> 1];	
}

uint16_t Get_ADC(void)
{
	
  // Запуск преобразования
	uint16_t v[ADC_COUNT];	
	uint16_t ref[ADC_COUNT];	
	
	for (int i=0; i< ADC_COUNT; i++)
	{
		HAL_ADC_Start(&hadc);		
		if (HAL_ADC_PollForConversion(&hadc, 10) == HAL_OK)
			v[i] = HAL_ADC_GetValue(&hadc);		
		HAL_ADC_Start(&hadc);		
		if (HAL_ADC_PollForConversion(&hadc, 10) == HAL_OK)
			ref[i] = HAL_ADC_GetValue(&hadc);		
	}
	
	v_ref = GetMedium(ref, ADC_COUNT);
	uint16_t ans = GetMedium(v, ADC_COUNT);
	return ans;
}

uint8_t temp_to_str(int temp, char* str)
{
	uint8_t dig = 0;
	str[0]=0;
	
	if(temp < 0) 
	{ 
		str[dig] = '-'; 
		temp *= -1; 
		dig = 1;		
	}
	if(temp>=100) 
	{ 
		str[dig] = '1'; 
		temp -=100; 
		dig = 1;		
	}
	
	char d = '0' + temp/10;
	if (str[0]=='1' || d!='0')
		str[dig++] = d;
		
	temp = temp % 10;
	str[dig++] = '0' + temp;
		
	return dig;
}

void WriteTemp(int temp)
{
	#define WIDTH        	28
	#define HEIGHT 				32
	
	char buff[3];
	uint8_t dig = temp_to_str(temp, buff);
	for(int i=0; i<dig; i++)
	{
		ssd1306_SetCursor(WIDTH * i, 0);
		myChar(buff[i], Medium, WIDTH, HEIGHT);		
	}	
	
}

uint8_t mV_to_str(int mV, char* str)
{
	
	str[0] = '0' + mV / 10000;
	mV %= 10000;
	
	str[1] = '0' + mV / 1000;
	mV %= 1000;
	
	str[2] = '.';
	
	str[3] = '0' + mV / 100;
	
	return 4;
}

void WriteVolt(int mV)
{
	char buff[4];
	uint8_t dig = mV_to_str(mV, buff);
	for(int i=0; i<dig; i++)
	{
		ssd1306_SetCursor(96 + 7 * i, 0);
		myChar(buff[i], SmallFont, 7, 8);		
	}	
}

int GetTemp(void)
{
	uint8_t temp[2];
	HAL_I2C_Mem_Read(&hi2c1, DS1621_I2C_ADDR, DS1621_READ, 1, temp, 2, HAL_MAX_DELAY);
	return (int8_t)temp[0];
	
}

int Get_mV(void)
{
	uint16_t adc = Get_ADC();
	uint64_t mV = (uint64_t)VREF * (uint64_t)adc *(uint64_t)(GND_RES + POW_RES);
	mV /= ((uint64_t)v_ref * (uint64_t)GND_RES);
	
	return mV;
}

void DS1621_Init(void)
{	
	uint8_t cfg;
	  
	HAL_I2C_Mem_Read(&hi2c1, DS1621_I2C_ADDR, DS1621_CFG, 1, &cfg, 1, HAL_MAX_DELAY);
	HAL_I2C_Mem_Write(&hi2c1, DS1621_I2C_ADDR, DS1621_CFG, 1, &cfg, 1, HAL_MAX_DELAY);
	cfg = DS1621_START;
	HAL_I2C_Master_Transmit(&hi2c1, DS1621_I2C_ADDR, &cfg, 1, HAL_MAX_DELAY);
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

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC_Init();
  MX_I2C1_Init();
  MX_LPTIM1_Init();
  /* USER CODE BEGIN 2 */
	DS1621_Init();
	ssd1306_Init();	
	
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
		int temp = GetTemp();
		int mV = Get_mV();
		
		ssd1306_Fill(Black);
		WriteTemp(temp);
		WriteVolt(mV);
		
		ssd1306_UpdateScreen();
		HAL_Delay(UPDATE_MS);
		if(temp >= 110)
		{
			ssd1306_Fill(White);
			ssd1306_UpdateScreen();
			HAL_Delay(UPDATE_MS);
			ssd1306_Fill(Black);
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
