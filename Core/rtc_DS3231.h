#ifndef __RTC_DS3231_H__
#define __RTC_DS3231_H__

#include <stddef.h>

// ===== user settings =====
#include "stm32l011_my_hal.h"
#define DS3231_I2C_WRITE(reg, data, size) i2c_write(I2C1, DS3231_ADDR, &reg, 1, data, size) // Задать функцию записи
#define DS3231_I2C_READ(reg, data, size)  i2c_read (I2C1, DS3231_ADDR, &reg, 1, data, size)// Задать функцию чтения

// ===== DRIVER =====
/*
Alarm 1
DY/DT 	A1M4 	A1M3 	A1M2 	A1M1 	ALARM RATE
X 		1 		1		1		1 		Alarm once per second
X 		1 		1		1		0 		Alarm when seconds match
X 		1 		1		0		0 		Alarm when minutes and seconds match
X 		1 		0		0		0 		Alarm when hours, minutes, and seconds match
0 		0 		0		0		0	 	Alarm when date, hours, minutes, and seconds match
1 		0 		0		0		0 		Alarm when day, hours, minutes, and seconds match

Alarm 2
DY/DT	A2M4 	A2M3 	A2M2			ALARM RATE
X 		1 		1 		1 				Alarm once per minute (00 seconds of every minute)
X 		1 		1 		0 				Alarm when minutes match
X 		1 		0 		0 				Alarm when hours and minutes match
0 		0 		0 		0 				Alarm when date, hours, and minutes match
1 		0 		0 		0 				Alarm when day, hours, and minutes match
*/
typedef struct
{
	// timer
	uint8_t seconds				:4;	// Секунды (0–9), младший разряд в BCD-формате
	uint8_t seconds_10			:3; // Секунды (0–5), старший разряд (десятки) в BCD-формате
	uint8_t seconds_rfu			:1; // Зарезервировано (должно быть 0)

	uint8_t minutes				:4; // Минуты (0–9), младший разряд в BCD
	uint8_t minutes_10			:3; // Минуты (0–5), десятки в BCD
	uint8_t minutes_rfu			:1; // Зарезервировано (должно быть 0)

	uint8_t hours				:4; // Часы (0–9), младший разряд в BCD
	uint8_t hours_10			:2; // Часы (0–2), десятки в BCD (макс. 2 для 24-часового режима)
	uint8_t hours_12			:1; // 1 = 12-часовой режим, 0 = 24-часовой режим
	uint8_t hours_rfu			:1; // Зарезервировано (должно быть 0)

	uint8_t day					:3; // День недели (1–7, где 1 = воскресенье по умолчанию)
	uint8_t day_rfu				:5; // Зарезервировано (должно быть 0)

	uint8_t date				:4; // День месяца (1–9), младший разряд в BCD
	uint8_t date_10				:2; // День месяца (0–3), десятки в BCD (макс. 31)
	uint8_t date_rfu			:2; // Зарезервировано (должно быть 0)

	uint8_t month				:4; // Месяц (1–9), младший разряд в BCD
	uint8_t month_10			:1; // Месяц (0–1), десятки в BCD (макс. 12 → 1)
	uint8_t month_rfu			:2; // Зарезервировано (должно быть 0)
	uint8_t century				:1; // Флаг столетия: 1 = 20xx, 0 = 19xx (переключается при переходе 99→00)

	uint8_t year				:4; // Год (0–9), младший разряд в BCD (например, 25 для 2025)
	uint8_t year_10				:4; // Год (0–9), десятки в BCD (например, 2 для 2025 → 25)

	// alarms
	uint8_t a1_seconds			:4; // Секунды будильника 1 (0–9), BCD
	uint8_t a1_seconds_10		:3;	// Десятки секунд будильника 1 (0–5), BCD
	uint8_t a1_m1				:1;	// Бит маски A1M1: 1 = игнорировать секунды при сравнении

	uint8_t a1_minutes			:4; // Минуты будильника 1 (0–9), BCD
	uint8_t a1_minutes_10		:3; // Десятки минут будильника 1 (0–5), BCD
	uint8_t a1_m2				:1; // Бит маски A1M2: 1 = игнорировать минуты

	uint8_t a1_hours			:4; // Часы будильника 1 (0–9), BCD
	uint8_t a1_hours_10			:2; // Десятки часов будильника 1 (0–2), BCD
	uint8_t a1_hours_12			:1; // Режим времени будильника: 1 = 12-часовой, 0 = 24-часовой
	uint8_t a1_m3				:1; // Бит маски A1M3: 1 = игнорировать часы

	uint8_t a1_date				:4; // День (месяца или недели) для будильника 1, BCD
	uint8_t a1_date_10			:2; // Десятки дня (0–3), BCD
	uint8_t a1_dy_dt			:1; // 1 = день недели, 0 = день месяца
	uint8_t a1_m4				:1; // Бит маски A1M4: 1 = игнорировать дату/день недели

	uint8_t a2_minutes			:4; // Минуты будильника 2 (0–9), BCD
	uint8_t a2_minutes_10		:3; // Десятки минут будильника 2 (0–5), BCD
	uint8_t a2_m2				:1; // Бит маски A2M2: 1 = игнорировать минуты

	uint8_t a2_hours			:4; // Часы будильника 2 (0–9), BCD
	uint8_t a2_hours_10			:2; // Десятки часов будильника 2 (0–2), BCD
	uint8_t a2_hours_12			:1; // Режим времени будильника 2: 1 = 12-часовой, 0 = 24-часовой
	uint8_t a2_m3				:1; // Бит маски A2M3: 1 = игнорировать часы

	uint8_t a2_date				:4; // День (месяца или недели) для будильника 2, BCD
	uint8_t a2_date_10			:2; // Десятки дня (0–3), BCD
	uint8_t a2_dy_dt			:1; // 1 = день недели, 0 = день месяца
	uint8_t a2_m4				:1; // Бит маски A2M4: 1 = игнорировать дату/день недели

	// config — регистр Control (0x0E)
	uint8_t a1_ie				:1; // Alarm 1 Interrupt Enable: 1 = разрешить прерывание от будильника 1
	uint8_t a2_ie				:1; // Alarm 2 Interrupt Enable: 1 = разрешить прерывание от будильника 2
	uint8_t int_cn				:1; // Interrupt Control: 1 = использовать вывод INT как прерывание, 0 = как square wave
	uint8_t rs1					:1; // Rate Select 1 — частота SQW (вместе с rs2)
	uint8_t rs2					:1; // Rate Select 2 // RS2:RS1 = 00 → 1Hz, 01 → 1.024kHz, 10 → 4.096kHz, 11 → 8.192kHz
	uint8_t conv				:1; // Convert Temperature: 1 = запуск однократного измерения температуры
	uint8_t bbsqw				:1; // Battery-Backed Square-Wave: 1 = SQW сохраняется при питании от батареи
	uint8_t EOSC				:1; // Enable Oscillator: 1 = остановить генератор, 0 = запустить (активный низкий!)

	// config — регистр Status (0x0F)
	uint8_t a1f					:1; // Alarm 1 Flag: 1 = сработал будильник 1 (сбрасывается записью 0)
	uint8_t a2f					:1; // Alarm 2 Flag: 1 = сработал будильник 2
	uint8_t bsy					:1; // Busy: 1 = идёт запись в EEPROM температурного компенсатора (нельзя читать/писать)
	uint8_t en32khz				:1; // Enable 32kHz Output: 1 = включить выход 32kHz на пине 32KHZ
	uint8_t cfg_rfu				:3; // Зарезервировано (должно быть 0)
	uint8_t osf					:1; // Oscillator Stop Flag: 1 = генератор останавливался (потеря точности)

	// Aging Offset Register (0x10)
	uint8_t aging_offset			:8; // Значение коррекции частоты (в единицах ~0.1 ppm). Кастовать в int8_t

	// Temperature Registers (0x11–0x12)
	uint8_t temperature_MSB		:8; // Старший байт температуры: биты 15–8 (в формате signed int) // Например: 0x19 = +25°C, 0xFF = -1°C

	uint8_t temperature_LSB_rfu :6; // Младшие 6 бит — зарезервированы (всегда 0)
	uint8_t temperature_LSB		:2; // Младшие 2 бита температуры (биты 7–6): 00 = .0°C, 01 = .25°C, 10 = .5°C, 11 = .75°C
} DS3231_t;

#define DS3231_ADDR (0x68<<1)

int DS3231_Read(DS3231_t *time);

int DS3231_Write(DS3231_t *time);

int DS3231_Write_byte(uint8_t reg, uint8_t byte);

uint32_t DS3231_date_to_sec(const DS3231_t * const time);

int DS3231_correct(uint8_t new_hour, uint8_t new_minutes, uint8_t new_seconds, uint32_t *last_correct_sec);

#endif //__RTC_DS3231_H__
