/**
 * @file stm32l011_my_hal.c
 * @brief Реализация пользовательского HAL для STM32L011.
 *
 * Содержит реализации функций для работы с I2C, EEPROM, ADC, SysTick и IWDG.
 *
 * Логика работы:
 * - I2C: Функции реализуют стандартные сценарии записи (Write) и записи-чтения (Write-Read)
 *   по протоколу I2C с проверкой состояния шины и таймаутом.
 * - EEPROM: Функции разблокируют/блокируют EEPROM, проверяют границы,
 *   и выполняют побайтовую/многобайтовую запись/чтение.
 * - ADC: Настройка и запуск одиночного преобразования на указанный канал.
 * - SysTick: Использует TIM2 для генерации прерываний каждую мс и инкремента глобальной переменной.
 * - IWDG: Настройка сторожевого таймера на максимальный возможный таймаут.
 */

#include "stm32l011_my_hal.h"


// ===== I2C =====

/**
 * @brief Макрос для проверки состояния I2C и обработки ошибок/таймаута.
 *
 * Проверяет флаги ошибок (NACK, ARLO, BERR) и таймаут.
 * Если флаг установлен или таймаут истёк, возвращает соответствующий код ошибки.
 * Используется внутри функций i2c_write и i2c_read.
 *
 * @param I2Cn Указатель на периферию I2C (например, I2C1).
 */
#define _CHECK_I2C_STATE(I2Cn) do { \
		  	  	  	  	  	  	if (--timeout == 0) return -5; \
		        				if (LL_I2C_IsActiveFlag_NACK(I2Cn)) { LL_I2C_ClearFlag_NACK(I2Cn); return -2; } \
		        				if (LL_I2C_IsActiveFlag_ARLO(I2Cn)) { LL_I2C_ClearFlag_ARLO(I2Cn); return -3; } \
		        				if (LL_I2C_IsActiveFlag_BERR(I2Cn)) { LL_I2C_ClearFlag_BERR(I2Cn); return -4; } \
		        				} while(0)

/**
 * @brief Реализация записи данных в устройство по шине I2C.
 *
 * Выполняет сценарий: START -> (addr + WRITE) -> [reg_bytes] -> [data_bytes] -> STOP.
 *
 * @param[in] I2Cx Указатель на I2C периферию.
 * @param[in] addr 7-битный адрес устройства, сдвинутый влево.
 * @param[in] reg Указатель на буфер с адресом регистра.
 * @param[in] reg_size Размер адреса регистра.
 * @param[in] buff Указатель на буфер с данными.
 * @param[in] size Размер данных.
 * @return 1 при успехе, отрицательное значение при ошибке (-2: NACK, -3: ARLO, -4: BERR, -5: Timeout).
 */
int i2c_write(I2C_TypeDef *I2Cx, uint8_t addr, uint8_t *reg, uint16_t reg_size, uint8_t *buff, uint16_t size)
{
    // Проверки на валидность аргументов
    if (!buff && !reg) return 0; // Нечего делать
    if (!reg_size && !size) return 0; // Нечего делать

    // Инициировать передачу: START + адрес + WRITE
    LL_I2C_HandleTransfer(I2Cx, addr, LL_I2C_ADDRSLAVE_7BIT, reg_size + size, LL_I2C_MODE_AUTOEND, LL_I2C_GENERATE_START_WRITE);

    // Передать байты регистра
    for (uint16_t i = 0; i < reg_size; i++)
    {
    	uint32_t timeout = 10000;
    	while (!LL_I2C_IsActiveFlag_TXIS(I2Cx)) // Ждать, пока буфер передатчика готов
    		_CHECK_I2C_STATE(I2Cx);
    	LL_I2C_TransmitData8(I2Cx, reg[i]);
    }

    // Передать байты данных
    for (uint16_t i = 0; i < size; i++)
    {
    	uint32_t timeout = 10000;
        while (!LL_I2C_IsActiveFlag_TXIS(I2Cx)) // Ждать, пока буфер передатчика готов
        	_CHECK_I2C_STATE(I2Cx);
        LL_I2C_TransmitData8(I2Cx, buff[i]);
    }

    // Дождаться флага STOP (передача завершена)
    uint32_t timeout = 10000;
    while (!LL_I2C_IsActiveFlag_STOP(I2Cx))
    	_CHECK_I2C_STATE(I2Cx);
    // Сбросить флаг STOP
    LL_I2C_ClearFlag_STOP(I2Cx);

    return 1; // успех
}

/**
 * @brief Реализация чтения данных из устройства по шине I2C.
 *
 * Выполняет сценарий: START -> (addr + WRITE) -> [reg_bytes] -> RESTART -> (addr + READ) -> [data_bytes] -> STOP.
 * Если reg_size = 0, пропускается этап записи регистра.
 *
 * @param[in] I2Cx Указатель на I2C периферию.
 * @param[in] addr 7-битный адрес устройства, сдвинутый влево.
 * @param[in] reg Указатель на буфер с адресом регистра.
 * @param[in] reg_size Размер адреса регистра.
 * @param[out] buff Указатель на буфер для данных.
 * @param[in] size Размер данных для чтения.
 * @return 1 при успехе, отрицательное значение при ошибке (-1: неверные аргументы, -2: NACK, -3: ARLO, -4: BERR, -5: Timeout).
 */
int i2c_read(I2C_TypeDef *I2Cx, uint8_t addr, uint8_t *reg, uint16_t reg_size, uint8_t *buff, uint16_t size)
{
    // Проверки на валидность аргументов
    if ((!reg && reg_size > 0) || (!buff && size > 0)) {
        return -1; // Неверные аргументы
    }
    if (reg_size == 0 && size == 0) {
        return 0; // Нечего делать — успех
    }

    uint32_t timeout;

    // Этап 1: Запись адреса регистра (если требуется)
    if (reg_size > 0)
    {
        // Начать передачу адреса регистра
        LL_I2C_HandleTransfer(I2Cx, addr, LL_I2C_ADDRSLAVE_7BIT, reg_size, LL_I2C_MODE_SOFTEND, LL_I2C_GENERATE_START_WRITE);

        // Передать байты регистра
        for (uint16_t i = 0; i < reg_size; i++)
        {
            timeout = 10000;
            while (!LL_I2C_IsActiveFlag_TXIS(I2Cx)) // Ждать, пока буфер передатчика готов
                _CHECK_I2C_STATE(I2Cx);
            LL_I2C_TransmitData8(I2Cx, reg[i]);
        }

        // Дождаться завершения передачи регистра (флаг TC)
        timeout = 10000;
        while (!LL_I2C_IsActiveFlag_TC(I2Cx))
            _CHECK_I2C_STATE(I2Cx);
    }

    // Этап 2: Чтение данных
    if (size > 0)
    {
        // Начать чтение данных (с RESTART, если писали регистр, или с START, если нет)
        LL_I2C_HandleTransfer(I2Cx, addr, LL_I2C_ADDRSLAVE_7BIT, size, LL_I2C_MODE_AUTOEND, reg_size > 0 ? LL_I2C_GENERATE_RESTART_7BIT_READ : LL_I2C_GENERATE_START_READ);

        // Прочитать байты данных
        for (uint16_t i = 0; i < size; i++)
        {
            timeout = 10000;
            while (!LL_I2C_IsActiveFlag_RXNE(I2Cx)) // Ждать, пока буфер приёмника готов
                _CHECK_I2C_STATE(I2Cx);
            buff[i] = LL_I2C_ReceiveData8(I2Cx);
        }

        // Дождаться флага STOP (чтение завершено)
        timeout = 10000;
        while (!LL_I2C_IsActiveFlag_STOP(I2Cx))
            _CHECK_I2C_STATE(I2Cx);
        // Сбросить флаг STOP
        LL_I2C_ClearFlag_STOP(I2Cx);
    }

    return 1; // успех
}

// ===== EEPROM =====

/// @brief Базовый адрес EEPROM в памяти.
#define EEPROM_BASE_ADDR        (0x08080000U)
/// @brief Размер EEPROM в байтах.
#define EEPROM_SIZE             (512U)
/// @brief Ключи для разблокировки EEPROM.
#define EEPROM_PEKEY1           (0x89ABCDEFU)
#define EEPROM_PEKEY2           (0x02030405U)

/**
 * @brief Внутренняя функция разблокировки EEPROM.
 * Использует ключи для снятия блокировки записи.
 */
static void EEPROM_Unlock(void)
{
    if (FLASH->PECR & FLASH_PECR_PELOCK)
    {
        FLASH->PEKEYR = EEPROM_PEKEY1;
        FLASH->PEKEYR = EEPROM_PEKEY2;
    }
}

/**
 * @brief Внутренняя функция блокировки EEPROM.
 * Устанавливает бит PELOCK.
 */
static void EEPROM_Lock(void)
{
    FLASH->PECR |= FLASH_PECR_PELOCK;
}

// --- Чтение ---

/**
 * @brief Чтение байта из EEPROM.
 * @param[in] offset Смещение в EEPROM.
 * @return Прочитанный байт.
 */
uint8_t EEPROM_ReadByte(uint32_t offset)
{
    return *(volatile uint8_t *)(EEPROM_BASE_ADDR + offset);
}

/**
 * @brief Чтение полуслова (16 бит) из EEPROM.
 * @param[in] offset Смещение в EEPROM.
 * @return Прочитанное полуслово.
 */
uint16_t EEPROM_ReadHalfWord(uint32_t offset)
{
	return *(volatile uint16_t *)(EEPROM_BASE_ADDR + offset);
}

/**
 * @brief Чтение слова (32 бита) из EEPROM.
 * @param[in] offset Смещение в EEPROM.
 * @return Прочитанное слово.
 */
uint32_t EEPROM_ReadWord(uint32_t offset)
{
	return *(volatile uint32_t *)(EEPROM_BASE_ADDR + offset);
}

// --- Запись ---

/**
 * @brief Запись блока данных в EEPROM.
 * @param[in] offset Смещение в EEPROM для начала записи.
 * @param[in] data Указатель на буфер с данными.
 * @param[in] size Размер данных для записи.
 */
void EEPROM_Write_data(uint32_t offset, uint8_t* data, uint32_t size)
{
	 // Проверка границ и указателя
	 if (!data || offset >= EEPROM_SIZE || offset + size >= EEPROM_SIZE) return;

	EEPROM_Unlock();
	while(size--)
	{
		// Сброс флагов ошибок перед записью
		FLASH->SR = FLASH_SR_EOP | FLASH_SR_WRPERR | FLASH_SR_PGAERR;
		// Запись байта
		*(volatile uint8_t *)(EEPROM_BASE_ADDR + offset) = *(data++);
		// Ожидание завершения записи
		while (FLASH->SR & FLASH_SR_BSY);
	}
	EEPROM_Lock();
}

/**
 * @brief Запись байта в EEPROM.
 * @param[in] offset Смещение в EEPROM.
 * @param[in] data Байт для записи.
 */
void EEPROM_WriteByte(uint32_t offset, uint8_t data)
{
    EEPROM_Unlock();
    // Сброс флагов ошибок
    FLASH->SR = FLASH_SR_EOP | FLASH_SR_WRPERR | FLASH_SR_PGAERR;
    // Запись байта
    *(volatile uint8_t *)(EEPROM_BASE_ADDR + offset) = data;
    // Ожидание завершения записи
    while (FLASH->SR & FLASH_SR_BSY);
    EEPROM_Lock();
}

/**
 * @brief Запись полуслова (16 бит) в EEPROM.
 * @param[in] offset Смещение в EEPROM.
 * @param[in] data Полуслово для записи.
 */
void EEPROM_WriteHalfWord(uint32_t offset, uint16_t data)
{
    EEPROM_Unlock();
    FLASH->SR = FLASH_SR_EOP | FLASH_SR_WRPERR | FLASH_SR_PGAERR;
    *(volatile uint16_t *)(EEPROM_BASE_ADDR + offset) = data;
    while (FLASH->SR & FLASH_SR_BSY);
    EEPROM_Lock();
}

/**
 * @brief Запись слова (32 бита) в EEPROM.
 * @param[in] offset Смещение в EEPROM.
 * @param[in] data Слово для записи.
 */
void EEPROM_WriteWord(uint32_t offset, uint32_t data)
{
    EEPROM_Unlock();
    FLASH->SR = FLASH_SR_EOP | FLASH_SR_WRPERR | FLASH_SR_PGAERR;
    *(volatile uint32_t *)(EEPROM_BASE_ADDR + offset) = data;
    while (FLASH->SR & FLASH_SR_BSY);
    EEPROM_Lock();
}

// ===== ADC =====

/**
 * @brief Чтение значения с указанного канала АЦП.
 * @param[in] channel Канал АЦП (например, LL_ADC_CHANNEL_VREFINT).
 * @return Значение АЦП (12-битное).
 */
uint16_t Read_ADC_Channel(uint32_t channel) //  LL_ADC_CHANNEL_X или LL_ADC_CHANNEL_VREFINT или LL_ADC_CHANNEL_TEMPSENSOR
{
    // Установить канал для одиночного преобразования
    LL_ADC_REG_SetSequencerChannels(ADC1, channel);

    // Запустить преобразование
    LL_ADC_REG_StartConversion(ADC1);

    // Дождаться завершения
    while (!LL_ADC_IsActiveFlag_EOC(ADC1));

    // Сбросить флаг завершения
    LL_ADC_ClearFlag_EOC(ADC1);

    // Прочитать результат
    return LL_ADC_REG_ReadConversionData12(ADC1);
}

// ===== ms_timer =====

/// @brief Глобальная переменная для счёта миллисекунд.
volatile uint32_t timer_ms = 0;

/**
 * @brief Инициализация таймера TIM2 для отсчёта миллисекунд.
 *
 * Настройка TIM2 для генерации прерывания каждую 1 мс.
 * Использует SystemCoreClock для расчёта предделителя.
 */
void timer_ms_init()	// пока для tim2
{
    // 1. Включить тактирование TIM2
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    // 2. Сбросить регистры TIM2
    RCC->APB1RSTR |= RCC_APB1RSTR_TIM2RST;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_TIM2RST;
    // 3. Рассчитать предделитель для тика 1 мкс
    uint32_t prescaler = SystemCoreClock / 1000000U - 1U;
    // 4. Настроить таймер: предделитель, период (ARR = 999 -> 1000 тиков = 1 мс)
    TIM2->PSC = prescaler;     // Prescaler
    TIM2->ARR = 999;           // Auto-reload: 1000 тиков = 1 мс
    TIM2->EGR = TIM_EGR_UG;    // Generate update event to reload PSC/ARR
    TIM2->SR = 0;              // Clear all status flags
    // 5. Включить прерывание по обновлению (UIE)
    TIM2->DIER |= TIM_DIER_UIE;
    // 6. Запустить таймер
    TIM2->CR1 |= TIM_CR1_CEN;
    // 7. Включить прерывание в NVIC
    NVIC_EnableIRQ(TIM2_IRQn);
}

/**
 * @brief Обработчик прерывания от TIM2.
 * Инкрементирует глобальную переменную timer_ms.
 */
void TIM2_IRQHandler(void)
{
    if (TIM2->SR & TIM_SR_UIF) // Проверить флаг прерывания по обновлению
    {
        TIM2->SR &= ~TIM_SR_UIF; // Сбросить флаг
        timer_ms++;              // Инкремент счётчика
    }
}

// ===== Disable boot =====

/// @brief Ключи для разблокировки Option Bytes.
#define OPT_PEKEY1           (0xFBEAD9C8U)
#define OPT_PEKEY2           (0x24252627U)

/**
 * @brief Настройка Option Bytes для загрузки из Flash (не из System Memory).
 * @note Эта функция в текущей реализации закомментирована, так как
 *       изменение Option Bytes требует специфичных условий и может быть
 *       небезопасно. Рекомендуется использовать внешние утилиты программатора.
 */
void SetOptionBytes_For_FlashBoot(void)
{
	// Функция не работает в текущем виде. Рекомендуется использовать STM32CubeProgrammer.
	/*
    if ((FLASH->OPTR & 0xE0000000U) == 0x60000000U) return; // Проверка, если уже установлено

    // Разблокировка EEPROM и Option Bytes
    FLASH->PEKEYR = 0x89ABCDEFU;
    FLASH->PEKEYR = 0x02030405U;
    FLASH->OPTKEYR = OPT_PEKEY1;
    FLASH->OPTKEYR = OPT_PEKEY2;

    // Новое значение OPTR: nBOOT1=1, nBOOT0=1, nBOOT_SEL=1
    uint32_t new_optr = (FLASH->OPTR & 0x1FFFFFFFU) | 0x60000000U;

    FLASH->OPTR = new_optr; // Запись нового значения
    while (FLASH->SR & FLASH_SR_BSY); // Ожидание завершения

    FLASH->PECR |= FLASH_PECR_OBL_LAUNCH; // Запрос сброса для применения изменений

    // Завершение выполнения (ожидается сброс МК)
    while (1); // Это не обязательно, после OBL_LAUNCH МК должен сброситься */
}

// ===== IWDG =====

/**
 * @brief Запуск сторожевого таймера (IWDG) с максимальным таймаутом.
 *
 * Устанавливает максимальный предделитель (256) и максимальное значение
 * перезагрузки (4095), что даёт таймаут ~32.7 секунды (при LSI ~37KHz).
 */
void IWDG_Start_MaxTimeout(void)
{
    // 1. Разрешить доступ к регистрам IWDG
    IWDG->KR = 0x5555;

    // 2. Установить максимальный предделитель (PR = 0b111 -> 256)
    IWDG->PR = IWDG_PR_PR_2 | IWDG_PR_PR_1 | IWDG_PR_PR_0;

    // 3. Установить максимальное значение перезагрузки (RLR = 0xFFF -> 4095)
    IWDG->RLR = 0xFFF;

    // 4. Перезагрузить счётчик IWDG
    IWDG->KR = 0xAAAA;

    // 5. Запустить IWDG (отключить нельзя после этого!)
    IWDG->KR = 0xCCCC;
}

/**
 * @brief Обновление (перезапуск) сторожевого таймера (IWDG).
 * "Поглаживание собаки" - предотвращает сброс МК.
 */
void IWDG_Refresh(void)
{
    IWDG->KR = 0xAAAA; // Команда перезагрузки счётчика
}