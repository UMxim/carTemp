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
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define CLOCK_UPDATE_PERIOD_MS 			500
#define BUTTON_UPDATE_PERIOD_MS 		10
#define BUTTON_LONG_PRESS_COUNT_LIMIT	(10000/BUTTON_UPDATE_PERIOD_MS) // 10 сек
#define BUTTON_PRESS_COUNT_LIMIT	    (100/BUTTON_UPDATE_PERIOD_MS) // 100 мсек

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
volatile uint32_t timer_ms_ = 0;

enum button_state {BUTTON_IDLE = 0, BUTTON_PRESS, BUTTON_LPRESS};
enum_button_name {BUTTON_LEFT =0, BUTTON_RIGHT, BUTTON_RESET};

struct
{
	// clock
	DS3231_t time;
	timer_t tim_update_clock;	
	// display
	uint8_t is_change;
	// button
	timer_t tim_update_button;	
	enum button_state button_state[BUTTON_RESET+1];	
}cache;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void delay_ms(uint32_t ms)
{
	timer_t tim;
	Timer_set(&tim, timer_ms_, ms);
	while(!Timer_isExpired(&tim, timer_ms_));
}

int ssd1306_i2c_write(uint8_t reg, uint8_t*buff, uint16_t size)
{
	return i2c_write(I2C1, SSD1306_I2C_ADDR, &reg, sizeof(reg), buff, size);
}


// ===== clock =====
void Clock_init()
{
	Timer_set(&cache.tim_update_clock, timer_ms_, TIMER_UPDATE_CLOCK_PERIOD_MS);

}

int Clock_edit()
{
	static uint8_t state = 0;
	switch (state)
	{
		case 0:
			if (cache.button_state[BUTTON_RESET] == BUTTON_LPRESS)
			{				
				cache.button_state[BUTTON_RESET] = BUTTON_IDLE;
				state = 1;
			}
			break;
		case 1:
			
	}
	return state;
}

void Clock_cycle()
{
	static uint8_t cnt = 0;
	static char time_str[6] = {[5]=0};
	int res = 0;	
	if (Timer_isExpired(&cache.tim_update_clock, timer_ms_))
	{
		if (Clock_edit()) return; // режим настройки
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
		cache.is_change = 1;
	}
	
}

// ===== button =====

void Button_init()
{
	Timer_set(&cache.tim_update_button, timer_ms_, TIMER_UPDATE_BUTTON_PERIOD_MS);
}

void Button_cycle()
{
	static uint32_t button_counter[BUTTON_RESET+1] = {0};
	uint8_t button_press[BUTTON_RESET+1];
	
	if (Timer_isExpired(&cache.tim_update_button, timer_ms_))
	{
		button_press[BUTTON_LEFT] = (левая нога нажата) ? 1 : 0;
		button_press[BUTTON_RIGHT] = (правая нога нажата) ? 1 : 0;
		button_press[BUTTON_RESET] = (ресет нога нажата) ? 1 : 0;
		
		for (int i=BUTTON_LEFT; i<=BUTTON_RESET; i++)
		{		
			if (button_press[i]==1)
				cache.button_counter[i]++;
			else 
			{
				if ((cache.button_counter[i] > BUTTON_PRESS_COUNT_LIMIT) &&
					(cache.button_state[i] != BUTTON_LPRESS;))
					cache.button_state[i] = BUTTON_PRESS;
				cache.button_counter[i] = 0;
			}
			if (cache.button_counter[i] >= BUTTON_LONG_PRESS_COUNT_LIMIT)
				cache.button_state[i] = BUTTON_LPRESS;
		}			
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
  delay_ms(100);
  ssd1306_Init(ssd1306_i2c_write, delay_ms);



  //ssd1306_DrawCircle(10, 10, 5, SSD1306_COLOR_WHITE);
  //ssd1306_UpdateScreen();

  ssd1306_Fill(SSD1306_COLOR_BLACK);
  ssd1306_SetCursor(0, 0);
  ssd1306_WriteString("22:54", 1, 0);
  ssd1306_SetCursor(80, 0);
  ssd1306_WriteString(" 12.5v", 0, 0);
  ssd1306_SetCursor(80, 16);
  ssd1306_WriteString("  -28c", 0, 1);
  ssd1306_UpdateScreen();

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

  volatile uint8_t wr = 0;
  DS3231_Read(&cache.time);
  Timer_set(&cache.tim_update_screen, timer_ms_, 500);
  Clock_init();
  Button_init();
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  Clock_cycle();
	  Button_cycle();
	  ssd1306_UpdateScreen();
	  if (wr)
	  {
		  DS3231_Write_byte(0x10, wr);
		  DS3231_Write_byte(0x0E, 0x20);

	  }
	  while(!Timer_isExpired(&cache.tim_update_screen, timer_ms_));
	  DS3231_Read(&cache.time);
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
