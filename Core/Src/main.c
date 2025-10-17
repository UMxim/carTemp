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
#include "tim.h"
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

#define R_HI_V_ENG						102458 // В омах - верхний(на питании) резистор на Veng
#define R_LO_V_ENG						12458
#define R_HI_V_BAT						102458
#define R_LO_V_BAT						12458
#define R_HI_V_LIGHT					102458
#define R_LO_V_LIGHT					12458

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
enum button_name {BUTTON_LEFT =0, BUTTON_RIGHT, BUTTON_RESET};

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
	enum button_state button_state[BUTTON_RESET+1];	
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
			if (cache.button_state[BUTTON_RESET] == BUTTON_LPRESS)
			{				
				cache.button_state[BUTTON_RESET] = BUTTON_IDLE;
				new_hour = cache.time.hours_10 * 10 + cache.time.hours;
				new_minutes = cache.time.minutes_10 * 10 + cache.time.minutes;
				state = 1;
			}
			break;
		case 1:
			if (cache.button_state[BUTTON_LEFT] == BUTTON_PRESS)
			{
				if (new_hour < 23) new_hour++ ; else new_hour = 0;
				cache.button_state[BUTTON_LEFT] = BUTTON_IDLE;
				need_update = 1;
			}
			
			if (cache.button_state[BUTTON_RIGHT] == BUTTON_PRESS)
			{
				if (new_minutes < 59) new_minutes++ ; else new_minutes = 0;
				cache.button_state[BUTTON_RIGHT] = BUTTON_IDLE;
				need_update = 1;
			}
				
			if (cache.button_state[BUTTON_RESET] == BUTTON_PRESS)
			{
				uint32_t last_correct_sec = EEPROM_ReadWord(0);// Прочитать из еепррма
				int res = DS3231_correct(new_hour, new_minutes, 0, &last_correct_sec);
				if (res > 0 ) EEPROM_WriteWord(0, last_correct_sec); // а тут записать в еепром
				res = DS3231_Read(&cache.time);		
				cache.button_state[BUTTON_RESET] = BUTTON_IDLE;
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
	return state;
}

void Clock_cycle()
{
	static uint8_t cnt = 0;
	static char time_str[6] = {[5]=0};
	static uint8_t is_init = 0;
	if (!is_init)
	{
		Timer_set(&cache.tim_update_clock, Systick_get_counter(), CLOCK_UPDATE_PERIOD_MS);
		is_init = 1;
	}

	int res = 0;
	if (Clock_edit()) return; // режим настройки
	if (Timer_isExpired(&cache.tim_update_clock, Systick_get_counter()))
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
		Timer_set(&cache.tim_update_button, Systick_get_counter(), BUTTON_UPDATE_PERIOD_MS);
		is_init = 1;
	}
	static uint32_t button_counter[BUTTON_RESET+1] = {0};
	uint8_t button_press[BUTTON_RESET+1];
	
	if (Timer_isExpired(&cache.tim_update_button, Systick_get_counter()))
	{
		button_press[BUTTON_LEFT] =  !LL_GPIO_IsInputPinSet(GPIOC, LL_GPIO_PIN_14);
		button_press[BUTTON_RIGHT] = !LL_GPIO_IsInputPinSet(GPIOC, LL_GPIO_PIN_15);
		button_press[BUTTON_RESET] = !LL_GPIO_IsInputPinSet(GPIOA, LL_GPIO_PIN_7);
		
		for (int i=BUTTON_LEFT; i<=BUTTON_RESET; i++)
		{		
			if (button_press[i])
				button_counter[i]++;
			else 
			{
				if ((button_counter[i] > BUTTON_PRESS_COUNT_LIMIT) &&
					(cache.button_state[i] != BUTTON_LPRESS))
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
		Timer_set(&cache.tim_update_screen, Systick_get_counter(), SCREEN_UPDATE_PERIOD_MS);
		is_init = 1;
	}
	if (!Timer_isExpired(&cache.tim_update_screen, Systick_get_counter()) ) return;
	// Яркость
	static uint8_t light = 1;
	if (cache.Vlight_mV > cache.Veng_mV) cache.Vlight_mV = cache.Veng_mV;
	uint8_t new_light = (cache.Vlight_mV > V_LO_THRESHOLD_LIGHT_mV) ? 0xFF - ( (cache.Vlight_mV - V_LO_THRESHOLD_LIGHT_mV) * 0xFF / (cache.Veng_mV - V_LO_THRESHOLD_LIGHT_mV) ) :	// подсветка включена. от 8 до 14 вольт
						(cache.Veng_mV > V_LO_THRESHOLD_mV) ? 0xFF : 	// Едем но без фар
						0x01;	// На батарейке
	uint8_t delta = (new_light > light) ? new_light - light : light - new_light;
	if(delta > 10) // 4%
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
	#warning (correct channels num)
	const uint32_t channels_[size_] = {LL_ADC_CHANNEL_1, LL_ADC_CHANNEL_3, LL_ADC_CHANNEL_11, LL_ADC_CHANNEL_VREFINT}; //
	static uint16_t adc[size_][ADC_AVRG_NUM] = {0};
	static uint8_t curr = 0;

	static uint8_t is_init = 0;
	if (!is_init)
	{
		Timer_set(&cache.tim_update_adc, Systick_get_counter(), ADC_UPDATE_PERIOD_MS / ADC_AVRG_NUM);
		is_init = 1;
	}
	if (!Timer_isExpired(&cache.tim_update_adc, Systick_get_counter())) return;
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
		Timer_set(&cache.tim_update_voltage, Systick_get_counter(), VOLTAGE_UPDATE_PERIOD_MS);
		is_init = 1;
	}
	if (!Timer_isExpired(&cache.tim_update_voltage, Systick_get_counter())) return;
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
		Timer_set(&cache.tim_update_temperature, Systick_get_counter(), TEMPERATURE_UPDATE_PERIOD_MS);
		ds1621_cfg_t cfg = {0};
		DS1621_set_cfg(&cfg);
		Timer_delay_ms(10);
		DS1621_start_convert();
		is_init = 1;
	}
	if (!Timer_isExpired(&cache.tim_update_temperature, Systick_get_counter())) return;
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
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
  __enable_irq();
  ssd1306_Init();
//  SetOptionBytes_For_FlashBoot();


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
  cache.time = (DS3231_t){
      // === Текущее время и дата ===
      .seconds      = 0,  // Секунды (0–9), младший разряд в BCD
      .seconds_10   = 0,  // Секунды (0–5), десятки в BCD
      .seconds_rfu  = 0,  // Зарезервировано (должно быть 0)

      .minutes      = 8,  // Минуты (0–9), младший разряд в BCD
      .minutes_10   = 2,  // Минуты (0–5), десятки в BCD
      .minutes_rfu  = 0,  // Зарезервировано (должно быть 0)

      .hours        = 3,  // Часы (0–9), младший разряд в BCD
      .hours_10     = 2,  // Часы (0–2), десятки в BCD (макс. 2 для 24-часового режима)
      .hours_12     = 0,  // 1 = 12-часовой режим, 0 = 24-часовой режим
      .hours_rfu    = 0,  // Зарезервировано (должно быть 0)

      .day          = 0,  // День недели (1–7, где 1 = воскресенье по умолчанию)
      .day_rfu      = 0,  // Зарезервировано (должно быть 0)

      .date         = 0,  // День месяца (1–9), младший разряд в BCD
      .date_10      = 0,  // День месяца (0–3), десятки в BCD (макс. 31)
      .date_rfu     = 0,  // Зарезервировано (должно быть 0)

      .month        = 0,  // Месяц (1–9), младший разряд в BCD
      .month_10     = 0,  // Месяц (0–1), десятки в BCD (макс. 12 → 1)
      .month_rfu    = 0,  // Зарезервировано (должно быть 0)
      .century      = 0,  // Флаг столетия: 1 = 20xx, 0 = 19xx

      .year         = 0,  // Год (0–9), младший разряд в BCD (например, 5 для 2025)
      .year_10      = 0,  // Год (0–9), десятки в BCD (например, 2 для 2025 → 25)

      // === Будильник 1 ===
      .a1_seconds      = 0,  // Секунды будильника 1 (0–9), BCD
      .a1_seconds_10   = 0,  // Десятки секунд будильника 1 (0–5), BCD
      .a1_m1           = 0,  // Бит маски A1M1: 1 = игнорировать секунды

      .a1_minutes      = 0,  // Минуты будильника 1 (0–9), BCD
      .a1_minutes_10   = 0,  // Десятки минут будильника 1 (0–5), BCD
      .a1_m2           = 0,  // Бит маски A1M2: 1 = игнорировать минуты

      .a1_hours        = 0,  // Часы будильника 1 (0–9), BCD
      .a1_hours_10     = 0,  // Десятки часов будильника 1 (0–2), BCD
      .a1_hours_12     = 0,  // Режим времени будильника: 1 = 12-часовой, 0 = 24-часовой
      .a1_m3           = 0,  // Бит маски A1M3: 1 = игнорировать часы

      .a1_date         = 0,  // День (месяца или недели) для будильника 1, BCD
      .a1_date_10      = 0,  // Десятки дня (0–3), BCD
      .a1_dy_dt        = 0,  // 1 = день недели, 0 = день месяца
      .a1_m4           = 0,  // Бит маски A1M4: 1 = игнорировать дату/день недели

      // === Будильник 2 ===
      .a2_minutes      = 0,  // Минуты будильника 2 (0–9), BCD
      .a2_minutes_10   = 0,  // Десятки минут будильника 2 (0–5), BCD
      .a2_m2           = 0,  // Бит маски A2M2: 1 = игнорировать минуты

      .a2_hours        = 0,  // Часы будильника 2 (0–9), BCD
      .a2_hours_10     = 0,  // Десятки часов будильника 2 (0–2), BCD
      .a2_hours_12     = 0,  // Режим времени будильника 2: 1 = 12-часовой, 0 = 24-часовой
      .a2_m3           = 0,  // Бит маски A2M3: 1 = игнорировать часы

      .a2_date         = 0,  // День (месяца или недели) для будильника 2, BCD
      .a2_date_10      = 0,  // Десятки дня (0–3), BCD
      .a2_dy_dt        = 0,  // 1 = день недели, 0 = день месяца
      .a2_m4           = 0,  // Бит маски A2M4: 1 = игнорировать дату/день недели

      // === Регистр Control (0x0E) ===
      .a1_ie        = 0,  // Alarm 1 Interrupt Enable: 1 = разрешить прерывание от будильника 1
      .a2_ie        = 0,  // Alarm 2 Interrupt Enable: 1 = разрешить прерывание от будильника 2
      .int_cn       = 0,  // Interrupt Control: 1 = INT как прерывание, 0 = как square wave
      .rs1          = 0,  // Rate Select 1 (вместе с rs2): частота SQW
      .rs2          = 0,  // Rate Select 2
      .conv         = 1,  // Convert Temperature: 1 = запуск измерения температуры
      .bbsqw        = 0,  // Battery-Backed SQW: 1 = SQW активен при питании от батареи
      .EOSC         = 0,  // Enable Oscillator: 1 = остановить генератор, 0 = запустить

      // === Регистр Status (0x0F) ===
      .a1f          = 0,  // Alarm 1 Flag: 1 = сработал будильник 1 (сбрасывается записью 0)
      .a2f          = 0,  // Alarm 2 Flag: 1 = сработал будильник 2
      .bsy          = 0,  // Busy: 1 = идёт запись в EEPROM (нельзя читать/писать)
      .en32khz      = 0,  // Enable 32kHz Output: 1 = включить выход 32kHz
      .cfg_rfu      = 0,  // Зарезервировано (должно быть 0)
      .osf          = 0,  // Oscillator Stop Flag: 1 = генератор останавливался

      // === Aging Offset (0x10) ===
      .aging_offset = 0,  // Значение коррекции частоты (~0.1 ppm)

      // === Температура (0x11–0x12) ===
      .temperature_MSB      = 0,  // Старший байт температуры (биты 15–8)

      .temperature_LSB_rfu  = 0,  // Зарезервировано (всегда 0)
      .temperature_LSB      = 0,  // Младшие 2 бита температуры: 00=.0°C, 01=.25°C, 10=.5°C, 11=.75°C
  };




  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	//  Clock_cycle();
	  Button_cycle();
	//  ADC_cycle();
	//  Voltage_cycle();
	//  Temperature_cycle();
	//  Display_cycle();
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
