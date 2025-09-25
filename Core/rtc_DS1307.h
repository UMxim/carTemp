#ifndef __RTC_DS1307_H__
#define __RTC_DS1307_H__

#include <stddef.h>

// ===== user settings =====
#include "i2c.h"
#define DS1307_I2C_WRITE(reg, data, size) i2c_write(I2C1, DS1307_ADDR, &reg, 1, data, size) // Задать функцию записи
#define DS1307_I2C_READ(data, size)       i2c_read (I2C1, DS1307_ADDR, &reg, 1, data, size)// Задать функцию чтения

// ===== DRIVER =====
typedef struct
{
	uint8_t seconds				:4;	// секунды - младшая цифра
	uint8_t seconds_10			:3; // секунды -старшая цифра (десятки)
	uint8_t ch					:1; // clock halt - остановка часов если 1

	uint8_t minutes				:4; // минуты
	uint8_t minutes_10			:3; // десятки минут
	uint8_t minutes_rfu			:1; // не используется

	uint8_t hours				:4; // часы
	uint8_t hours_10			:2; // десятки часов
	uint8_t hours_12			:1; // 12 или 24 режим. при 1 - 12часовой режим
	uint8_t hours_rfu			:1; // не используется

	uint8_t day					:3; // дни недели
	uint8_t day_rfu				:5; // не используется

	uint8_t date				:4; // дни месяца
	uint8_t date_10				:2; // десятки
	uint8_t date_rfu			:2; // не используется

	uint8_t month				:4; // месяц
	uint8_t month_10			:1; // десятки
	uint8_t month_rfu			:3; // не используется

	uint8_t year				:4; // год
	uint8_t year_10				:4; // десятки

	uint8_t cfg_rs				:2;	// Частота вывода генератора. 0(1Hz), 1(4096Hz), 2(8192Hz), 3(32768Hz)
	uint8_t cfg_rfu_0			:2; // не используется
	uint8_t cfg_sqwe			:1; // На ноге 1(генератор) или 0(просто уровень)
	uint8_t cfg_rfu_1			:2; // не используется
	uint8_t cfg_out				:1; // Если уровень, то задается здесь. 0(lo) или 1(hi)
}DS1307_t;

#define DS1307_ADDR (0x68<<1)

static inline int DS1307_Read(DS1307_t *time)
{
	uint8_t reg = 0;
	return DS1307_I2C_READ((uint8_t*)time, sizeof(DS1307_t));
}

static inline int DS1307_Write(DS1307_t *time)
{
	uint8_t reg = 0;
	return DS1307_I2C_WRITE(reg, (uint8_t *)time, sizeof(DS1307_t));
}

static inline int DS1307_Write_byte(uint8_t reg, uint8_t byte)
{
	return DS1307_I2C_WRITE(reg, &byte, 1);
}

#endif //__RTC_DS1307_H__
