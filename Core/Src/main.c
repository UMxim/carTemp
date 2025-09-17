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
volatile uint32_t timer_ms_ = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
int i2c_write(uint8_t addr, uint8_t *buff, uint16_t size)
{
    if (!buff || size == 0) return -1;

    // Старт + адрес устройства (запись)
    LL_I2C_HandleTransfer(I2C1, addr, LL_I2C_ADDRSLAVE_7BIT,
                          size, LL_I2C_MODE_AUTOEND, LL_I2C_GENERATE_START_WRITE);

    for (uint16_t i = 0; i < size; i++)
    {
        // Ждём, пока можно передавать
        while (!LL_I2C_IsActiveFlag_TXIS(I2C1))
        {
            // Проверка на ошибки (NACK, Arbitration Lost, Bus Error)
            if (LL_I2C_IsActiveFlag_NACK(I2C1))
            {
                LL_I2C_ClearFlag_NACK(I2C1);
                return -2; // NACK — устройство не ответило
            }
            if (LL_I2C_IsActiveFlag_ARLO(I2C1))
            {
                LL_I2C_ClearFlag_ARLO(I2C1);
                return -3; // Arbitration Lost
            }
            if (LL_I2C_IsActiveFlag_BERR(I2C1))
            {
                LL_I2C_ClearFlag_BERR(I2C1);
                return -4; // Bus Error
            }
        }

        // Передаём байт
        LL_I2C_TransmitData8(I2C1, buff[i]);
    }

    // Ждём STOP
    while (!LL_I2C_IsActiveFlag_STOP(I2C1));

    // Сбрасываем флаг STOP
    LL_I2C_ClearFlag_STOP(I2C1);

    return 0; // успех
}

int i2c_read(uint8_t addr, uint8_t reg, uint8_t *buff, uint16_t size)
{
    if (!buff || size == 0) return -1;

    // Этап 1: Запись регистра (без STOP — будет Repeated Start)
    LL_I2C_HandleTransfer(I2C1, addr, LL_I2C_ADDRSLAVE_7BIT,
                          1, LL_I2C_MODE_SOFTEND, LL_I2C_GENERATE_START_WRITE);

    // Ждём TXIS и передаём адрес регистра
    uint32_t timeout = 10000;
    while (!LL_I2C_IsActiveFlag_TXIS(I2C1))
    {
        if (--timeout == 0) return -5; // таймаут
        if (LL_I2C_IsActiveFlag_NACK(I2C1)) { LL_I2C_ClearFlag_NACK(I2C1); return -2; }
        if (LL_I2C_IsActiveFlag_ARLO(I2C1)) { LL_I2C_ClearFlag_ARLO(I2C1); return -3; }
        if (LL_I2C_IsActiveFlag_BERR(I2C1)) { LL_I2C_ClearFlag_BERR(I2C1); return -4; }
    }
    LL_I2C_TransmitData8(I2C1, reg);

    // Ждём TC (Transfer Complete) — конец передачи 1 байта без STOP
    timeout = 10000;
    while (!LL_I2C_IsActiveFlag_TC(I2C1))
    {
        if (--timeout == 0) return -5;
        if (LL_I2C_IsActiveFlag_NACK(I2C1)) { LL_I2C_ClearFlag_NACK(I2C1); return -2; }
        if (LL_I2C_IsActiveFlag_ARLO(I2C1)) { LL_I2C_ClearFlag_ARLO(I2C1); return -3; }
        if (LL_I2C_IsActiveFlag_BERR(I2C1)) { LL_I2C_ClearFlag_BERR(I2C1); return -4; }
    }

    // Этап 2: Чтение данных с Repeated Start
    LL_I2C_HandleTransfer(I2C1, addr, LL_I2C_ADDRSLAVE_7BIT,
                          size, LL_I2C_MODE_AUTOEND, LL_I2C_GENERATE_RESTART_7BIT_READ);

    // Читаем байты
    for (uint16_t i = 0; i < size; i++)
    {
        timeout = 10000;
        while (!LL_I2C_IsActiveFlag_RXNE(I2C1)) // Данные готовы к чтению
        {
            if (--timeout == 0) return -5;
            if (LL_I2C_IsActiveFlag_NACK(I2C1)) { LL_I2C_ClearFlag_NACK(I2C1); return -2; }
            if (LL_I2C_IsActiveFlag_ARLO(I2C1)) { LL_I2C_ClearFlag_ARLO(I2C1); return -3; }
            if (LL_I2C_IsActiveFlag_BERR(I2C1)) { LL_I2C_ClearFlag_BERR(I2C1); return -4; }
        }
        buff[i] = LL_I2C_ReceiveData8(I2C1);
    }

    // Ждём STOP (автоматически сгенерирован в AUTOEND)
    timeout = 10000;
    while (!LL_I2C_IsActiveFlag_STOP(I2C1))
    {
        if (--timeout == 0) return -5;
    }
    LL_I2C_ClearFlag_STOP(I2C1);

    return 0; // успех
}

void delay_ms(uint32_t ms)
{
	uint32_t timestamp = timer_ms_;
	while(timer_ms_ - timestamp < ms);
}

int ssd1306_i2c_write(uint8_t*buff, uint16_t size)
{
	return i2c_write(SSD1306_I2C_ADDR, buff, size);
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
  delay_ms(1000);
  ssd1306_Init(ssd1306_i2c_write, delay_ms);



    ssd1306_DrawPixel(5, 5, 1);
  ssd1306_UpdateScreen();
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
