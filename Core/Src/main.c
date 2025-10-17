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
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "misc.h"
#include "ssd1306.h"
#include "rtc_DS3231.h"
#include "ds1621.h"
#include "stm32l011_my_hal.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define CLOCK_UPDATE_PERIOD_MS 			500
#define VOLTAGE_UPDATE_PERIOD_MS 		200
#define TEMPERATURE_UPDATE_PERIOD_MS 	1000

#define BUTTON_UPDATE_PERIOD_MS 		10
#define ADC_UPDATE_PERIOD_MS 			100 // полный цикл. Если смотрим из 8 измерений - разделим на 8
#define SCREEN_UPDATE_PERIOD_MS 		100

#define R_HI_V_ENG						83200 // В омах - верхний(на питании) резистор на Veng
#define R_LO_V_ENG						19860
#define R_HI_V_BAT						82400
#define R_LO_V_BAT						19890
#define R_HI_V_LIGHT					81800
#define R_LO_V_LIGHT					19940

#define V_BAT_HI_WARNING_mV				14600 // опасно высокое напряжение
#define V_BAT_LO_WARNING_mV				12000 // опасно низкое напряжение
#define V_LO_THRESHOLD_mV				2000 // считаем что напряжения нет
#define V_LO_THRESHOLD_LIGHT_mV			8000 // Яркость изменяется от V_LO_THRESHOLD_LIGHT_mV до Vbat

#define T_HI_WARNING					106 // опасная температура для двигателя
#define T_LO_WARNING					0  // низкая температура для двигателя

#define ADC_AVRG_NUM					8 // количество измерений для вычисления медианы
#define BUTTON_LONG_PRESS_COUNT_LIMIT	(10000/BUTTON_UPDATE_PERIOD_MS) // 10 сек
#define BUTTON_PRESS_COUNT_LIMIT	    (100/BUTTON_UPDATE_PERIOD_MS) // 100 мсек

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

enum button_state {BUTTON_IDLE = 0, BUTTON_PRESS, BUTTON_LPRESS};
enum button_name {BUTTON_H =0, BUTTON_M, BUTTON_R};

struct
{	
	// clock
	DS3231_t time;
	timer_t tim_update_clock;	
	// voltage
	timer_t tim_update_voltage;
	// temperature
	timer_t tim_update_temperature;
	// button
	timer_t tim_update_button;	
	enum button_state button_state[BUTTON_R+1];
	// screen
	timer_t tim_update_screen;
	// adc
	timer_t tim_update_adc;
	uint32_t Veng_mV;
	uint32_t Vbat_mV;
	uint32_t Vlight_mV;


}cache;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */






// ===== clock =====

int Clock_edit()
{
	static uint8_t state = 0;
	static uint8_t new_hour = 0;
	static uint8_t new_minutes = 0;
	uint8_t need_update = 0;
	switch (state)
	{
		case 0:
			if (cache.button_state[BUTTON_R] == BUTTON_LPRESS)
			{
				new_hour = cache.time.hours_10 * 10 + cache.time.hours;
				new_minutes = cache.time.minutes_10 * 10 + cache.time.minutes;
				state = 1;
			}
			break;
		case 1:
			if (cache.button_state[BUTTON_H] == BUTTON_PRESS)
			{
				if (new_hour < 23) new_hour++ ; else new_hour = 0;
				need_update = 1;
			}
			
			if (cache.button_state[BUTTON_M] == BUTTON_PRESS)
			{
				if (new_minutes < 59) new_minutes++ ; else new_minutes = 0;
				need_update = 1;
			}
				
			if (cache.button_state[BUTTON_R] == BUTTON_PRESS)
			{
				uint32_t last_correct_sec = EEPROM_ReadWord(0);// Прочитать из еепррма
				int res = DS3231_correct(new_hour, new_minutes, 0, &last_correct_sec);
				if (res > 0 ) EEPROM_WriteWord(0, last_correct_sec); // а тут записать в еепром
				res = DS3231_Read(&cache.time);
				state = 0;
			}
			if (need_update)
			{
				static char time_str[6] = {[5]=0};
				time_str[0] = '0' + new_hour/10;
				time_str[1] = '0' + new_hour%10;
				time_str[2] = ' ';
				time_str[3] = '0' + new_minutes/10;
				time_str[4] = '0' + new_minutes%10;
				ssd1306_SetCursor(0, 0);
				ssd1306_WriteString(time_str, 1, 1);
			}
			break;
		default:
			break;
	}
	cache.button_state[BUTTON_R] = BUTTON_IDLE;
	cache.button_state[BUTTON_H] = BUTTON_IDLE;
	cache.button_state[BUTTON_M] = BUTTON_IDLE;
	return state;
}

void Clock_cycle()
{
	static uint8_t cnt = 0;
	static char time_str[6] = {[5]=0};
	static uint8_t is_init = 0;
	if (!is_init)
	{
		Timer_set(&cache.tim_update_clock, CLOCK_UPDATE_PERIOD_MS);
		is_init = 1;
	}

	static int res = 0;
	if (Clock_edit()) return; // режим настройки
	if (Timer_isExpired(&cache.tim_update_clock))
	{
		cnt++;
		if (cnt & 1)
			res = DS3231_Read(&cache.time);		
		if (res > 0)
		{
			time_str[0] = '0' + cache.time.hours_10;
			time_str[1] = '0' + cache.time.hours;
			time_str[2] = (cnt & 1) ? ' ' : ':';
			time_str[3] = '0' + cache.time.minutes_10;
			time_str[4] = '0' + cache.time.minutes;
		}
		else
		{
			time_str[0] = ':';
			time_str[1] = ':';
			time_str[2] = '0' - res;
			time_str[3] = ':';
			time_str[4] = ':';
		}
		ssd1306_SetCursor(0, 0);
		ssd1306_WriteString(time_str, 1, 0);
	}
}

// ===== button =====

void Button_cycle()
{
	static uint8_t is_init = 0;
	if (!is_init)
	{
		Timer_set(&cache.tim_update_button, BUTTON_UPDATE_PERIOD_MS);
		is_init = 1;
	}
	static uint32_t button_counter[BUTTON_R+1] = {0};
	uint8_t button_press[BUTTON_R+1];
	
	if (Timer_isExpired(&cache.tim_update_button))
	{
		button_press[BUTTON_H] =  !LL_GPIO_IsInputPinSet(GPIOC, LL_GPIO_PIN_14);
		button_press[BUTTON_M] = !LL_GPIO_IsInputPinSet(GPIOC, LL_GPIO_PIN_15);
		button_press[BUTTON_R] = !LL_GPIO_IsInputPinSet(GPIOA, LL_GPIO_PIN_7);
		
		for (int i=BUTTON_H; i<=BUTTON_R; i++)
		{		
			if (button_press[i])
				button_counter[i]++;
			else 
			{
				if ((button_counter[i] > BUTTON_PRESS_COUNT_LIMIT) && (button_counter[i] < BUTTON_LONG_PRESS_COUNT_LIMIT))
					cache.button_state[i] = BUTTON_PRESS;
				button_counter[i] = 0;
			}
			if (button_counter[i] >= BUTTON_LONG_PRESS_COUNT_LIMIT)
				cache.button_state[i] = BUTTON_LPRESS;
		}			
	}
}

// ===== screen =====

void Display_cycle()
{
	static uint8_t is_init = 0;
	if (!is_init)
	{
		ssd1306_Init();
		Timer_set(&cache.tim_update_screen, SCREEN_UPDATE_PERIOD_MS);
		is_init = 1;
	}
	if (!Timer_isExpired(&cache.tim_update_screen) ) return;
	// Яркость
	static uint8_t light = 1;
	if (cache.Vlight_mV > cache.Veng_mV) cache.Vlight_mV = cache.Veng_mV;
	uint8_t new_light = (cache.Vlight_mV > V_LO_THRESHOLD_LIGHT_mV) ? 0xFF - ( (cache.Vlight_mV - V_LO_THRESHOLD_LIGHT_mV) * 0xFF / (cache.Veng_mV - V_LO_THRESHOLD_LIGHT_mV) ) :	// подсветка включена. от 8 до 14 вольт
						(cache.Veng_mV > V_LO_THRESHOLD_mV) ? 0xFF : 	// Едем но без фар
						0x01;	// На батарейке
	uint8_t delta = (new_light > light) ? new_light - light : light - new_light;
	if(delta > 0) // 0%
	{
		ssd1306_SetContrast(light);
		light = new_light;
	}

	ssd1306_UpdateScreen();
}

// ===== adc =====

void ADC_cycle()
{
	enum channels_name { Veng = 0, Vbat, Vlight, Vref, size_};
	const uint32_t channels_[size_] = {LL_ADC_CHANNEL_0, LL_ADC_CHANNEL_1, LL_ADC_CHANNEL_4, LL_ADC_CHANNEL_VREFINT}; //
	static uint16_t adc[size_][ADC_AVRG_NUM] = {0};
	static uint8_t curr = 0;

	static uint8_t is_init = 0;
	if (!is_init)
	{
		Timer_set(&cache.tim_update_adc, ADC_UPDATE_PERIOD_MS / ADC_AVRG_NUM);
		is_init = 1;
	}
	if (!Timer_isExpired(&cache.tim_update_adc)) return;
	for (int i=Veng; i < size_; i++)
	{
		adc[i][curr] = Read_ADC_Channel(channels_[i]);
	}
	curr++;
	if (curr == ADC_AVRG_NUM) // собрали весь набор. обрабатываем
	{
		curr = 0;

		uint16_t V = GetMedian_16(&adc[Vref][0], ADC_AVRG_NUM); // медианное значение набора. переиспользуемая переменная
		uint32_t k = GET_ADC_K(ADC_V_REF_mV, V);

		V = GetMedian_16(&adc[Veng][0], ADC_AVRG_NUM);
		cache.Veng_mV = GET_mV(V, k) * (R_HI_V_ENG + R_LO_V_ENG) / R_LO_V_ENG;

		V = GetMedian_16(&adc[Vbat][0], ADC_AVRG_NUM);
		cache.Vbat_mV = GET_mV(V, k) * (R_HI_V_BAT + R_LO_V_BAT) / R_LO_V_BAT;

		V = GetMedian_16(&adc[Vlight][0], ADC_AVRG_NUM);
		cache.Vlight_mV = GET_mV(V, k) * (R_HI_V_LIGHT + R_LO_V_LIGHT) / R_LO_V_LIGHT;
	}

}

// ===== voltage =====

void Voltage_cycle()
{
	static uint8_t is_init = 0;
	if (!is_init)
	{
		Timer_set(&cache.tim_update_voltage, VOLTAGE_UPDATE_PERIOD_MS);
		is_init = 1;
	}
	if (!Timer_isExpired(&cache.tim_update_voltage)) return;
	uint8_t is_warning = (cache.Vbat_mV < V_BAT_LO_WARNING_mV) || (cache.Vbat_mV > V_BAT_HI_WARNING_mV) ? 1 : 0;
	char v_mV[12]; // "0123456789AB"
	Int_to_str(cache.Vbat_mV, v_mV);
	char v[7] =" 12.5v";
	v[4] = v_mV[9]; v[2] = v_mV[8]; v[1] = v_mV[7];

	ssd1306_SetCursor(80, 0);
	ssd1306_WriteString(v, 0, is_warning);
}

// ===== temperature =====

void Temperature_cycle()
{
	static uint8_t is_init = 0;
	if (!is_init)
	{
		Timer_set(&cache.tim_update_temperature, TEMPERATURE_UPDATE_PERIOD_MS);
		ds1621_cfg_t cfg = {0};
		DS1621_set_cfg(&cfg);
		Timer_delay_ms(10);
		DS1621_start_convert();
		is_init = 1;
	}
	if (!Timer_isExpired(&cache.tim_update_temperature)) return;
	ds1621_temp_t temp = DS1621_get_temp();
	uint8_t is_warning = (temp.temp <= T_LO_WARNING) || (temp.temp >= T_HI_WARNING) ? 1 : 0;
	char str_temp[12];
	Int_to_str(temp.temp, str_temp);
	char disp[7] ="  -33C";
	disp[5] = str_temp[11]; disp[4] = str_temp[10]; disp[3] = str_temp[9];
	ssd1306_SetCursor(80, 16);
	ssd1306_WriteString(disp, 0, is_warning);

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
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);

  /* SysTick_IRQn interrupt configuration */
  NVIC_SetPriority(SysTick_IRQn, 3);

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
  /* USER CODE BEGIN 2 */
  timer_ms_init();
//  SetOptionBytes_For_FlashBoot();
  volatile uint32_t temp = EEPROM_ReadWord(0);
  if (temp == 0 ) 	  EEPROM_WriteWord(0, 0xDEADBEEF);

  //ssd1306_DrawCircle(10, 10, 5, SSD1306_COLOR_WHITE);
  //ssd1306_UpdateScreen();


  //res = DS1307_Read(&cache.time);
  /*for (int y=0; y<32; y++)
	  for(int x=0; x<128; x++)
	  {
		  ssd1306_DrawPixel(x, y, SSD1306_COLOR_WHITE);
		  ssd1306_UpdateScreen();
	  }
*/
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  Clock_cycle();
	  Button_cycle();



	  ADC_cycle();
	//  Voltage_cycle();
	//  Temperature_cycle();
	  Display_cycle();
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  LL_FLASH_SetLatency(LL_FLASH_LATENCY_0);
  while(LL_FLASH_GetLatency()!= LL_FLASH_LATENCY_0)
  {
  }
  LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);
  while (LL_PWR_IsActiveFlag_VOS() != 0)
  {
  }
  LL_RCC_HSI_Enable();

   /* Wait till HSI is ready */
  while(LL_RCC_HSI_IsReady() != 1)
  {

  }
  LL_RCC_HSI_SetCalibTrimming(16);
  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
  LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_1);
  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSI);

   /* Wait till System clock is ready */
  while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSI)
  {

  }

  LL_Init1msTick(16000000);

  LL_SetSystemCoreClock(16000000);
  LL_RCC_SetI2CClockSource(LL_RCC_I2C1_CLKSOURCE_PCLK1);
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
