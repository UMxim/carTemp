/**
 * @file stm32l011_my_hal.h
 * @brief Заголовочный файл пользовательского HAL для STM32L011.
 *
 * Содержит объявления функций для работы с периферией (I2C, EEPROM, ADC, SysTick, IWDG),
 * которые не представлены в стандартной LL библиотеке или требуют упрощённого интерфейса.
 *
 * Логика работы:
 * - I2C: Функции i2c_write/read упрощают обмен данными с внешними устройствами
 *   по протоколу I2C, включая указание адреса регистра.
 * - EEPROM: Функции для чтения/записи данных в энергонезависимую память.
 * - ADC: Функция для чтения значения с указанного канала АЦП.
 * - SysTick: Инициализация и глобальная переменная для отсчёта времени в мс.
 * - IWDG: Управление сторожевым таймером.
 */

#ifndef STM32L011_MY_HAL_H_
#define STM32L011_MY_HAL_H_

#include <stddef.h>
#include <stdint.h>
#include "stm32l0xx_ll_i2c.h"
#include "stm32l0xx_ll_adc.h"
#include "stm32l0xx_ll_utils.h"
#include "misc.h"

// ===== I2C =====

/**
 * @brief Запись данных в устройство по шине I2C.
 *
 * Отправляет сначала адрес регистра (или команду), затем сами данные.
 *
 * @param[in] I2Cx Указатель на I2C периферию (например, I2C1).
 * @param[in] addr 7-битный адрес устройства, сдвинутый влево (LSB = 0).
 * @param[in] reg Указатель на буфер с адресом регистра для записи.
 * @param[in] reg_size Размер адреса регистра в байтах (обычно 1).
 * @param[in] buff Указатель на буфер с данными для записи.
 * @param[in] size Размер данных для записи в байтах.
 * @return Результат операции (обычно количество переданных байт или код ошибки).
 */
int i2c_write(I2C_TypeDef *I2Cx, uint8_t addr, uint8_t *reg, uint16_t reg_size, uint8_t *buff, uint16_t size);

/**
 * @brief Чтение данных из устройства по шине I2C.
 *
 * Сначала записывает адрес регистра, затем читает данные.
 *
 * @param[in] I2Cx Указатель на I2C периферию (например, I2C1).
 * @param[in] addr 7-битный адрес устройства, сдвинутый влево (LSB = 0).
 * @param[in] reg Указатель на буфер с адресом регистра для чтения.
 * @param[in] reg_size Размер адреса регистра в байтах (обычно 1).
 * @param[out] buff Указатель на буфер для получения данных.
 * @param[in] size Размер данных для чтения в байтах.
 * @return Результат операции (обычно количество полученных байт или код ошибки).
 */
int i2c_read(I2C_TypeDef *I2Cx, uint8_t addr, uint8_t *reg, uint16_t reg_size, uint8_t *buff, uint16_t size);

// ===== EEPROM =====

/**
 * @brief Чтение байта из EEPROM.
 *
 * @param[in] offset Смещение в EEPROM (адрес).
 * @return Прочитанный байт.
 */
uint8_t  EEPROM_ReadByte(uint32_t offset);

/**
 * @brief Чтение полуслова (16 бит) из EEPROM.
 *
 * @param[in] offset Смещение в EEPROM (адрес).
 * @return Прочитанное полуслово.
 */
uint16_t EEPROM_ReadHalfWord(uint32_t offset);

/**
 * @brief Чтение слова (32 бита) из EEPROM.
 *
 * @param[in] offset Смещение в EEPROM (адрес).
 * @return Прочитанное слово.
 */
uint32_t EEPROM_ReadWord(uint32_t offset);

/**
 * @brief Запись блока данных в EEPROM.
 *
 * @param[in] offset Смещение в EEPROM для начала записи.
 * @param[in] data Указатель на буфер с данными для записи.
 * @param[in] size Размер данных для записи в байтах.
 */
void EEPROM_Write_data(uint32_t offset, uint8_t* data, uint32_t size);

/**
 * @brief Запись байта в EEPROM.
 *
 * @param[in] offset Смещение в EEPROM (адрес).
 * @param[in] data Байт для записи.
 */
void EEPROM_WriteByte(uint32_t offset, uint8_t data);

/**
 * @brief Запись полуслова (16 бит) в EEPROM.
 *
 * @param[in] offset Смещение в EEPROM (адрес).
 * @param[in] data Полуслово для записи.
 */
void EEPROM_WriteHalfWord(uint32_t offset, uint16_t data);

/**
 * @brief Запись слова (32 бита) в EEPROM.
 *
 * @param[in] offset Смещение в EEPROM (адрес).
 * @param[in] data Слово для записи.
 */
void EEPROM_WriteWord(uint32_t offset, uint32_t data);

// ===== ADC =====

/**
 * @brief Чтение значения с указанного канала АЦП.
 *
 * @param[in] channel Канал АЦП (например, LL_ADC_CHANNEL_VREFINT).
 * @return Значение АЦП (обычно 12-битное).
 */
uint16_t Read_ADC_Channel(uint32_t channel);

/// @brief Опорное напряжение АЦП в мВ.
#define ADC_V_REF_mV	1224	// Значение Vref у этого МК

// ===== SysTick =====

/// @brief Глобальная переменная, содержащая количество миллисекунд с момента запуска.
extern volatile uint32_t timer_ms;

/**
 * @brief Инициализация таймера для отсчёта миллисекунд.
 *
 * Настроена на использование TIM2.
 */
void timer_ms_init();	// пока для tim2

/**
 * @brief Получение текущего значения счётчика миллисекунд.
 *
 * @return Текущее значение timer_ms.
 */
static inline uint32_t timer_ms_get_counter() {return timer_ms;}


uint32_t timer_get_mks();
/**
 * @brief Настройка Option Bytes для загрузки из Flash (не из System Memory).
 */
void SetOptionBytes_For_FlashBoot(void);

// ===== IWDG =====

/**
 * @brief Запуск сторожевого таймера (IWDG) с максимальным таймаутом.
 */
void IWDG_Start_MaxTimeout(void);

/**
 * @brief Обновление (перезапуск) сторожевого таймера (IWDG).
 */
void IWDG_Refresh(void);

#endif /* STM32L011_MY_HAL_H_ */
