#include "rtc_DS3231.h"

int DS3231_Read(DS3231_t *time)
{
	uint8_t reg = 0;
	return DS3231_I2C_READ(reg, (uint8_t*)time, sizeof(DS3231_t));
}

int DS3231_Write(DS3231_t *time)
{
	uint8_t reg = 0;
	return DS3231_I2C_WRITE(reg, (uint8_t *)time, sizeof(DS3231_t));
}

int DS3231_Write_byte(uint8_t reg, uint8_t byte)
{
	return DS3231_I2C_WRITE(reg, &byte, 1);
}

static inline uint8_t bcd2bin(uint8_t ones, uint8_t tens)
{
    return tens * 10 + ones;
}

// Проверка високосности (2000–2099)
static inline uint8_t is_leap(uint8_t year) {
    return (year & 3) == 0; // year % 4 == 0
}

// Дни в месяце
static uint8_t days_in_month(uint8_t month, uint8_t year) {
    const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && is_leap(year)) return 29;
    return days[month - 1];
}

// Увеличить дату на 1 день
static void date_inc(uint8_t *day, uint8_t *month, uint8_t *year) {
    (*day)++;
    if (*day > days_in_month(*month, *year)) {
        *day = 1;
        (*month)++;
        if (*month > 12) {
            *month = 1;
            (*year)++;
        }
    }
}

// Уменьшить дату на 1 день
static void date_dec(uint8_t *day, uint8_t *month, uint8_t *year) {
    if (*day > 1) {
        (*day)--;
    } else {
        if (*month == 1) {
            *month = 12;
            (*year)--;
        } else {
            (*month)--;
        }
        *day = days_in_month(*month, *year);
    }
}

uint32_t DS3231_date_to_sec(const DS3231_t * const time)
{
    uint8_t sec  = bcd2bin(time->seconds, time->seconds_10);
    uint8_t min  = bcd2bin(time->minutes, time->minutes_10);
    uint8_t hour = bcd2bin(time->hours, time->hours_10);

    uint8_t day  = bcd2bin(time->date, time->date_10);
    uint8_t mon  = bcd2bin(time->month, time->month_10);
    uint8_t year = bcd2bin(time->year, time->year_10);

    static const uint16_t days_before_month[] =
    {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };

    uint32_t days = (uint32_t)year * 365U + ((uint32_t)(year + 3U) >> 2);
    days += days_before_month[mon - 1];
    if (mon > 2 && (year & 3U) == 0)
    {
        days++;
    }
    days += (uint32_t)day - 1U;

    return days * 86400UL + (uint32_t)hour * 3600UL + (uint32_t)min * 60UL + (uint32_t)sec;
}

int DS3231_correct_with_date(uint8_t new_hour, uint8_t new_minutes, uint8_t new_seconds, uint32_t *last_correct_sec)
{
    if (new_hour > 23 || new_minutes > 59 || new_seconds > 59)
        return -1;

    DS3231_t time;
    int res = DS3231_Read(&time);
    if (res <= 0)
        return res;

    // Текущее время суток из RTC
    uint32_t sec_timer = bcd2bin(time.seconds, time.seconds_10)
                       + 60U * bcd2bin(time.minutes, time.minutes_10)
                       + 3600U * bcd2bin(time.hours, time.hours_10);
    uint32_t sec_new = new_hour * 3600U + new_minutes * 60U + new_seconds;

    // Нормализуем разницу
    int32_t delta_sec = (int32_t)sec_new - (int32_t)sec_timer;
    int8_t day_shift = 0;

    if (delta_sec > 43200) {
        // Новое время "намного больше" → вероятно, переход назад через полночь
        delta_sec -= 86400;
        day_shift = -1;
    } else if (delta_sec < -43200) {
        // Новое время "намного меньше" → переход вперёд через полночь
        delta_sec += 86400;
        day_shift = +1;
    }

    // Копируем текущую дату
    uint8_t day   = bcd2bin(time.date, time.date_10);
    uint8_t month = bcd2bin(time.month, time.month_10);
    uint8_t year  = bcd2bin(time.year, time.year_10);

    // Применяем сдвиг даты
    if (day_shift == +1) {
        date_inc(&day, &month, &year);
    } else if (day_shift == -1) {
        date_dec(&day, &month, &year);
    }

    // Собираем новую структуру
    DS3231_t time_new = time;
    time_new.seconds     = new_seconds % 10;
    time_new.seconds_10  = new_seconds / 10;
    time_new.minutes     = new_minutes % 10;
    time_new.minutes_10  = new_minutes / 10;
    time_new.hours       = new_hour % 10;
    time_new.hours_10    = new_hour / 10;

    // Обновляем дату в BCD
    time_new.date       = day % 10;
    time_new.date_10    = day / 10;
    time_new.month      = month % 10;
    time_new.month_10   = month / 10;
    time_new.year       = year % 10;
    time_new.year_10    = year / 10;

    // Абсолютное время ДО коррекции
    uint32_t old_abs = DS3231_date_to_sec(&time);
    // Абсолютное время ПОСЛЕ коррекции
    uint32_t new_abs = DS3231_date_to_sec(&time_new);

    // Проверка: delta_sec должно совпадать с (new_abs - old_abs)
    // (для отладки можно добавить assert)

    uint32_t elapsed = old_abs - *last_correct_sec;

    // Коррекция частоты (только при малой ошибке)
    if (delta_sec >= -1800 && delta_sec <= 1800 && elapsed >= 300) {
        int64_t pps = (int64_t)delta_sec * 10000000LL / (int64_t)elapsed;
        if (pps >= -128 && pps <= 127) {
            time_new.aging_offset = (uint8_t)(int8_t)pps;
        }
    }

    time_new.conv = 1;
    res = DS3231_Write(&time_new);
    if (res > 0) {
        *last_correct_sec = new_abs; // новое абсолютное время
    }
    return res;
}
