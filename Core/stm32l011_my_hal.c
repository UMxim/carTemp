/*
 * stm32l011_my_hal.c
 *
 *  Created on: Oct 7, 2025
 *      Author: myushkov
 */
#include "stm32l011_my_hal.h"


// ===== I2C =====

#define _CHECK_I2C_STATE(I2Cn) do { \
		  	  	  	  	  	  	if (--timeout == 0) return -5; \
		        				if (LL_I2C_IsActiveFlag_NACK(I2Cn)) { LL_I2C_ClearFlag_NACK(I2Cn); return -2; } \
		        				if (LL_I2C_IsActiveFlag_ARLO(I2Cn)) { LL_I2C_ClearFlag_ARLO(I2Cn); return -3; } \
		        				if (LL_I2C_IsActiveFlag_BERR(I2Cn)) { LL_I2C_ClearFlag_BERR(I2Cn); return -4; } \
		        				} while(0)

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

// ===== EEPROM =====

#define EEPROM_BASE_ADDR        (0x08080000U)
#define EEPROM_SIZE             (512U)
#define EEPROM_PEKEY1           (0x89ABCDEFU)
#define EEPROM_PEKEY2           (0x02030405U)


// Разблокировка EEPROM
static void EEPROM_Unlock(void)
{
    if (FLASH->PECR & FLASH_PECR_PELOCK)
    {
        FLASH->PEKEYR = EEPROM_PEKEY1;
        FLASH->PEKEYR = EEPROM_PEKEY2;
    }
}

// Блокировка EEPROM
static void EEPROM_Lock(void)
{
    FLASH->PECR |= FLASH_PECR_PELOCK;
}

// Чтение
uint8_t EEPROM_ReadByte(uint32_t offset)
{
    return *(volatile uint8_t *)(EEPROM_BASE_ADDR + offset);
}


uint16_t EEPROM_ReadHalfWord(uint32_t offset)
{
	return *(volatile uint16_t *)(EEPROM_BASE_ADDR + offset);
}

uint32_t EEPROM_ReadWord(uint32_t offset)
{
	return *(volatile uint32_t *)(EEPROM_BASE_ADDR + offset);
}

// Запись
void EEPROM_Write_data(uint32_t offset, uint8_t* data, int size)
{
	 if (!data || offset > EEPROM_SIZE || size > EEPROM_SIZE || offset + size > EEPROM_SIZE) return; // Защита от переполнения и NULL

	EEPROM_Unlock();
	while(size--)
	{
		FLASH->SR = FLASH_SR_EOP | FLASH_SR_WRPERR | FLASH_SR_PGAERR; // Сброс флагов
		*(volatile uint8_t *)(EEPROM_BASE_ADDR + offset) = *(data++);
		while (FLASH->SR & FLASH_SR_BSY); // Ждём завершения
	}
	EEPROM_Lock();
}

void EEPROM_WriteByte(uint32_t offset, uint8_t data)
{
    EEPROM_Unlock();
    FLASH->SR = FLASH_SR_EOP | FLASH_SR_WRPERR | FLASH_SR_PGAERR; // Сброс флагов
    *(volatile uint8_t *)(EEPROM_BASE_ADDR + offset) = data;
    while (FLASH->SR & FLASH_SR_BSY); // Ждём завершения
    EEPROM_Lock();
}

void EEPROM_WriteHalfWord(uint32_t offset, uint16_t data)
{
    EEPROM_Unlock();
    FLASH->SR = FLASH_SR_EOP | FLASH_SR_WRPERR | FLASH_SR_PGAERR;
    *(volatile uint16_t *)(EEPROM_BASE_ADDR + offset) = data;
    while (FLASH->SR & FLASH_SR_BSY);
    EEPROM_Lock();
}

void EEPROM_WriteWord(uint32_t offset, uint32_t data)
{
    EEPROM_Unlock();
    FLASH->SR = FLASH_SR_EOP | FLASH_SR_WRPERR | FLASH_SR_PGAERR;
    *(volatile uint32_t *)(EEPROM_BASE_ADDR + offset) = data;
    while (FLASH->SR & FLASH_SR_BSY);
    EEPROM_Lock();
}

// ===== ADC =====

uint16_t Read_ADC_Channel(uint32_t channel) //  LL_ADC_CHANNEL_X или LL_ADC_CHANNEL_VREFINT или LL_ADC_CHANNEL_TEMPSENSOR
{
    // Устанавливаем один канал (автоматически длина = 1)
    LL_ADC_REG_SetSequencerChannels(ADC1, channel);

    // Запуск преобразования
    LL_ADC_REG_StartConversion(ADC1);

    // Ждём окончания
    while (!LL_ADC_IsActiveFlag_EOC(ADC1));

    // Сброс флага EOC
    LL_ADC_ClearFlag_EOC(ADC1);

    // Чтение результата
    return LL_ADC_REG_ReadConversionData12(ADC1);
}

// ===== ms_timer =====

volatile uint32_t timer_ms = 0;

void timer_ms_init()	// пока для tim2
{
    // 1. Включить тактирование нужного таймера
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    // 2. Сбросить регистры таймера (опционально, но рекомендуется)
    RCC->APB1RSTR |= RCC_APB1RSTR_TIM2RST;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_TIM2RST;
    // 3. Рассчитать prescaler для 1 мкс тика
    // Требуемая частота таймера = 1 MHz → prescaler = SystemCoreClock / 1'000'000 - 1
    uint32_t prescaler = SystemCoreClock / 1000000U - 1U;
    // 4. Настроить таймер
    TIM2->PSC = prescaler;     // Prescaler
    TIM2->ARR = 999;           // Auto-reload: 1000 тиков = 1 мс (0..999 → 1000)
    TIM2->EGR = TIM_EGR_UG;    // Generate update event to reload PSC/ARR
    TIM2->SR = 0;              // Clear all status flags
    // 5. Включить прерывание по обновлению
    TIM2->DIER |= TIM_DIER_UIE;
    // 6. Запустить таймер
    TIM2->CR1 |= TIM_CR1_CEN;
    // 7. Включить прерывание в NVIC
    NVIC_EnableIRQ(TIM2_IRQn);
}

void TIM2_IRQHandler(void)
{
    if (TIM2->SR & TIM_SR_UIF)
    {
        TIM2->SR &= ~TIM_SR_UIF; // Clear update flag
        timer_ms++;
    }
}



// ===== Disable boot =====

#define OPT_PEKEY1           (0xFBEAD9C8U)
#define OPT_PEKEY2           (0x24252627U)

void SetOptionBytes_For_FlashBoot(void)
{
	// Чёта не работает( Крче надо в C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32CubeProgrammer.exe OptionsByte выставить nBoot_Sel = 1 и nBoot0 = 1
	/*
    if ((FLASH->OPTR & 0xE0000000U) == 0x60000000U) return; // nBOOT1=1, nBOOT0=1, nBOOT_SEL=1

    // Разблокировка
    FLASH->PEKEYR = 0x89ABCDEFU;
    FLASH->PEKEYR = 0x02030405U;
    FLASH->OPTKEYR = OPT_PEKEY1;
    FLASH->OPTKEYR = OPT_PEKEY2;

    // Новое значение: nBOOT1=1 (31), nBOOT0=1 (30), nBOOT_SEL=1 (29)
    uint32_t new_optr = (FLASH->OPTR & 0x1FFFFFFFU) | 0x60000000U;

    FLASH->OPTR = new_optr;
    while (FLASH->SR & FLASH_SR_BSY); // Ждём записи!

    FLASH->PECR |= FLASH_PECR_OBL_LAUNCH; // Запрос сброса

    // Больше НИЧЕГО не делаем — МК должен сброситься!*
    while (1); // На всякий случай, но обычно не нужно*/
}

