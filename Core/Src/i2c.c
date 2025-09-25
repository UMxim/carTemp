/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    i2c.c
  * @brief   This file provides code for the configuration
  *          of the I2C instances.
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
#include "i2c.h"

/* USER CODE BEGIN 0 */
#define _CHECK_I2C_STATE(I2Cn) do { \
		  	  	  	  	  	  	if (--timeout == 0) return -5; \
		        				if (LL_I2C_IsActiveFlag_NACK(I2Cn)) { LL_I2C_ClearFlag_NACK(I2Cn); return -2; } \
		        				if (LL_I2C_IsActiveFlag_ARLO(I2Cn)) { LL_I2C_ClearFlag_ARLO(I2Cn); return -3; } \
		        				if (LL_I2C_IsActiveFlag_BERR(I2Cn)) { LL_I2C_ClearFlag_BERR(I2Cn); return -4; } \
		        				} while(0)

/* USER CODE END 0 */

/* I2C1 init function */
void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  LL_I2C_InitTypeDef I2C_InitStruct = {0};

  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

  LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA);
  /**I2C1 GPIO Configuration
  PA9   ------> I2C1_SCL
  PA10   ------> I2C1_SDA
  */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_9;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_OPENDRAIN;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_UP;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_1;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LL_GPIO_PIN_10;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_OPENDRAIN;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_UP;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_1;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* Peripheral clock enable */
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C1);

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */

  /** I2C Initialization
  */
  LL_I2C_EnableAutoEndMode(I2C1);
  LL_I2C_DisableOwnAddress2(I2C1);
  LL_I2C_DisableGeneralCall(I2C1);
  LL_I2C_EnableClockStretching(I2C1);
  I2C_InitStruct.PeripheralMode = LL_I2C_MODE_I2C;
  I2C_InitStruct.Timing = 0x00303D5B;
  I2C_InitStruct.AnalogFilter = LL_I2C_ANALOGFILTER_ENABLE;
  I2C_InitStruct.DigitalFilter = 0;
  I2C_InitStruct.OwnAddress1 = 0;
  I2C_InitStruct.TypeAcknowledge = LL_I2C_ACK;
  I2C_InitStruct.OwnAddrSize = LL_I2C_OWNADDRESS1_7BIT;
  LL_I2C_Init(I2C1, &I2C_InitStruct);
  LL_I2C_SetOwnAddress2(I2C1, 0, LL_I2C_OWNADDRESS2_NOMASK);
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/* USER CODE BEGIN 1 */
int i2c_write(I2C_TypeDef *I2Cx, uint8_t addr, uint8_t *reg, uint16_t reg_size, uint8_t *buff, uint16_t size)
{
    if (!buff && !reg) return 0;
    if (!reg_size && !size) return 0;

    // Старт + адрес устройства (запись)
    LL_I2C_HandleTransfer(I2Cx, addr, LL_I2C_ADDRSLAVE_7BIT, reg_size + size, LL_I2C_MODE_AUTOEND, LL_I2C_GENERATE_START_WRITE);

    // Register
    for (uint16_t i = 0; i < reg_size; i++)
    {
    	uint32_t timeout = 10000;
    	while (!LL_I2C_IsActiveFlag_TXIS(I2Cx))// Ждём, пока можно передавать
    		_CHECK_I2C_STATE(I2Cx);
    	LL_I2C_TransmitData8(I2Cx, reg[i]);
    }

    // Data
    for (uint16_t i = 0; i < size; i++)
    {
    	uint32_t timeout = 10000;
        while (!LL_I2C_IsActiveFlag_TXIS(I2Cx))
        	_CHECK_I2C_STATE(I2Cx);
        LL_I2C_TransmitData8(I2Cx, buff[i]);
    }

    // Ждём STOP
    uint32_t timeout = 10000;
    while (!LL_I2C_IsActiveFlag_STOP(I2Cx))
    	_CHECK_I2C_STATE(I2Cx);
    // Сбрасываем флаг STOP
    LL_I2C_ClearFlag_STOP(I2Cx);

    return 1; // успех
}

int i2c_read(I2C_TypeDef *I2Cx, uint8_t addr, uint8_t *reg, uint16_t reg_size, uint8_t *buff, uint16_t size)
{
    // Проверка указателей
    if ((!reg && reg_size > 0) || (!buff && size > 0)) {
        return -1;
    }
    if (reg_size == 0 && size == 0) {
        return 0; // ничего не делаем — успех
    }

    uint32_t timeout;

    // Этап 1: Запись регистра (если есть)
    if (reg_size > 0)
    {
        LL_I2C_HandleTransfer(I2Cx, addr, LL_I2C_ADDRSLAVE_7BIT, reg_size, LL_I2C_MODE_SOFTEND, LL_I2C_GENERATE_START_WRITE);

        for (uint16_t i = 0; i < reg_size; i++)
        {
            timeout = 10000;
            while (!LL_I2C_IsActiveFlag_TXIS(I2Cx))
                _CHECK_I2C_STATE(I2Cx);
            LL_I2C_TransmitData8(I2Cx, reg[i]);
        }

        // Ждём TC (Transfer Complete) — конец передачи без STOP
        timeout = 10000;
        while (!LL_I2C_IsActiveFlag_TC(I2Cx))
            _CHECK_I2C_STATE(I2Cx);
    }

    // Этап 2: Чтение данных
    if (size > 0)
    {
        LL_I2C_HandleTransfer(I2Cx, addr, LL_I2C_ADDRSLAVE_7BIT, size, LL_I2C_MODE_AUTOEND, reg_size > 0 ? LL_I2C_GENERATE_RESTART_7BIT_READ : LL_I2C_GENERATE_START_READ);

        for (uint16_t i = 0; i < size; i++)
        {
            timeout = 10000;
            while (!LL_I2C_IsActiveFlag_RXNE(I2Cx)) // Ждём готовности данных
                _CHECK_I2C_STATE(I2Cx);
            buff[i] = LL_I2C_ReceiveData8(I2Cx);
        }

        // Ждём STOP
        timeout = 10000;
        while (!LL_I2C_IsActiveFlag_STOP(I2Cx))
            _CHECK_I2C_STATE(I2Cx);
        LL_I2C_ClearFlag_STOP(I2Cx);
    }

    return 1; // успех
}

/* USER CODE END 1 */
