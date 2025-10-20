/**
 * @file rtc_DS3231.c
 * @brief Реализация функций драйвера для RTC DS3231.
 *
 * Этот файл содержит реализацию функций для взаимодействия с микросхемой
 * реального времени DS3231, включая чтение/запись данных, конвертацию
 * даты/времени в/из секунд и функцию коррекции времени.
 *
 * Логика работы:
 * - DS3231_Read/Write: Считывают или записывают содержимое регистров RTC
 *   (начиная с регистра 0x00) в/из структуру DS3231_t.
 * - bcd2bin: Преобразует BCD-кодированные цифры (десятки и единицы) в двоичное число.
 * - date_inc/dec: Увеличивают/уменьшают дату на один день с учётом високосных лет.
 * - DS3231_date_to_sec: Преобразует дату/время из структуры DS3231_t в количество
 *   секунд с начала условного "эпохи" (2000 год).
 * - DS3231_correct: Сравнивает текущее время RTC с новым заданным временем,
 *   определяет разницу (включая возможный переход через полночь), корректирует
 *   дату, если нужно, и записывает новое время. Если ошибка небольшая и прошло
 *   достаточно времени с последней коррекции, также рассчитывает и записывает
 *   значение в регистр коррекции частоты (AGING) для компенсации температурного
 *   дрейфа.
 */

#include "rtc_DS3231.h"

/**
 * @brief Чтение всех регистров времени и конфигурации DS3231.
 *
 * Читает 19 байт данных из DS3231, начиная с регистра 0x00 (секунды),
 * и сохраняет их в структуре DS3231_t. Предполагается, что структура
 * точно соответствует формату регистров.
 *
 * @param[out] time Указатель на структуру DS3231_t для сохранения данных.
 * @return Результат операции I2C.
 */
int DS3231_Read(DS3231_t *time)
{
	uint8_t reg = 0;
	return DS3231_I2C_READ(reg, (uint8_t*)time, sizeof(DS3231_t));
}

/**
 * @brief Запись всех регистров времени и конфигурации DS3231.
 *
 * Записывает 19 байт данных из структуры DS3231_t в DS3231, начиная с регистра 0x00.
 * Предполагается, что структура точно соответствует формату регистров.
 *
 * @param[in] time Указатель на структуру DS3231_t с новыми данными.
 * @return Результат операции I2C.
 */
int DS3231_Write(DS3231_t *time)
{
	uint8_t reg = 0;
	return DS3231_I2C_WRITE(reg, (uint8_t *)time, sizeof(DS3231_t));
}

/**
 * @brief Запись одного байта в указанный регистр DS3231.
 *
 * @param[in] reg Адрес регистра для записи.
 * @param[in] byte Значение для записи.
 * @return Результат операции I2C.
 */
int DS3231_Write_byte(uint8_t reg, uint8_t byte)
{
	return DS3231_I2C_WRITE(reg, &byte, 1);
}

/**
 * @brief Преобразование BCD в двоичное число.
 *
 * Преобразует два BCD-цифра (единицы и десятки) в одно двоичное число.
 *
 * @param[in] ones BCD цифра единиц (0-9).
 * @param[in] tens BCD цифра десятков (0-9).
 * @return Результат в двоичном формате (например, ones=5, tens=2 -> 25).
 */
static inline uint8_t bcd2bin(uint8_t ones, uint8_t tens)
{
    return tens * 10 + ones;
}

// Проверка високосности (предполагается диапазон 2000–2099)
/**
 * @brief Проверка года на високосность (для диапазона 2000-2099).
 *
 * @param[in] year Год (0-99, соответствует 2000-2099).
 * @return 1, если год високосный, 0 - если нет.
 */
static inline uint8_t is_leap(uint8_t year) {
    // Для 2000-2099: високосный, если делится на 4
    return (year & 3) == 0; // year % 4 == 0
}

/**
 * @brief Получение количества дней в месяце.
 *
 * @param[in] month Месяц (1-12).
 * @param[in] year Год (0-99, соответствует 2000-2099).
 * @return Количество дней в месяце.
 */
static uint8_t days_in_month(uint8_t month, uint8_t year) {
    const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && is_leap(year)) return 29;
    return days[month - 1];
}

/**
 * @brief Увеличение даты на один день.
 *
 * Увеличивает дату (day, month, year), учитывая переходы через конец месяца и года.
 *
 * @param[in,out] day Указатель на день месяца (1-31).
 * @param[in,out] month Указатель на месяц (1-12).
 * @param[in,out] year Указатель на год (0-99, соответствует 2000-2099).
 */
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

/**
 * @brief Уменьшение даты на один день.
 *
 * Уменьшает дату (day, month, year), учитывая переходы через начало месяца и года.
 *
 * @param[in,out] day Указатель на день месяца (1-31).
 * @param[in,out] month Указатель на месяц (1-12).
 * @param[in,out] year Указатель на год (0-99, соответствует 2000-2099).
 */
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

/**
 * @brief Преобразование даты/времени DS3231 в количество секунд.
 *
 * Преобразует дату и время из структуры DS3231_t в абсолютное количество
 * секунд с начала условной эпохи (принято за 1 января 2000 года 00:00:00).
 * Использует алгоритм, учитывающий високосные годы.
 *
 * @param[in] time Указатель на структуру DS3231_t с датой/временем.
 * @return Количество секунд с начала эпохи.
 */
uint32_t DS3231_date_to_sec(const DS3231_t * const time)
{
    // Преобразуем BCD в двоичный формат
    uint8_t sec  = bcd2bin(time->seconds, time->seconds_10);
    uint8_t min  = bcd2bin(time->minutes, time->minutes_10);
    uint8_t hour = bcd2bin(time->hours, time->hours_10);

    uint8_t day  = bcd2bin(time->date, time->date_10);
    uint8_t mon  = bcd2bin(time->month, time->month_10);
    uint8_t year = bcd2bin(time->year, time->year_10);

    // Количество дней, прошедших до начала каждого месяца (в невисокосном году)
    static const uint16_t days_before_month[] =
    {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };

    // Вычисляем количество дней с начала эпохи
    // Упрощённый расчёт для 2000-2099: 365 дней в году + 1 день за каждые 4 года
    uint32_t days = (uint32_t)year * 365U + ((uint32_t)(year + 3U) >> 2);
    days += days_before_month[mon - 1];
    // Добавляем день за високосный год, если применимо
    if (mon > 2 && (year & 3U) == 0)
    {
        days++;
    }
    days += (uint32_t)day - 1U; // -1, потому что 1 января = 0 дней прошло

    // Вычисляем общее количество секунд
    return days * 86400UL + (uint32_t)hour * 3600UL + (uint32_t)min * 60UL + (uint32_t)sec;
}

/**
 * @brief Коррекция времени DS3231 на основе нового значения.
 *
 * Сравнивает текущее время из DS3231 с новым заданным временем (часы, минуты, секунды).
 * Определяет разницу, корректирует дату при необходимости (переход через полночь),
 * и записывает новое время обратно в DS3231. Если ошибка времени невелика и прошло
 * достаточно времени с последней коррекции, рассчитывает и записывает значение
 * в регистр коррекции частоты (AGING) для компенсации дрейфа.
 *
 * @param[in] new_hour Новое значение часов (0-23).
 * @param[in] new_minutes Новое значение минут (0-59).
 * @param[in] new_seconds Новое значение секунд (0-59).
 * @param[in,out] last_correct_sec Указатель на переменную, хранящую время последней
 *                                 успешной коррекции (в секундах, как в DS3231_date_to_sec).
 *                                 Обновляется при успешной коррекции.
 * @return Результат операции I2C (или -1 при ошибке валидации входных данных).
 */
int DS3231_correct(uint8_t new_hour, uint8_t new_minutes, uint8_t new_seconds, uint32_t *last_correct_sec)
{
    // Проверка валидности входных данных
    if (new_hour > 23 || new_minutes > 59 || new_seconds > 59)
        return -1;

    DS3231_t time;
    int res = DS3231_Read(&time);
    if (res <= 0)
        return res;

    // Текущее время суток из RTC (в секундах от начала дня)
    uint32_t sec_timer = bcd2bin(time.seconds, time.seconds_10)
                       + 60U * bcd2bin(time.minutes, time.minutes_10)
                       + 3600U * bcd2bin(time.hours, time.hours_10);
    // Новое время суток (в секундах от начала дня)
    uint32_t sec_new = new_hour * 3600U + new_minutes * 60U + new_seconds;

    // Нормализуем разницу, учитывая переход через полночь (24ч = 86400с)
    int32_t delta_sec = (int32_t)sec_new - (int32_t)sec_timer;
    int8_t day_shift = 0;

    if (delta_sec > 43200) { // 43200 = 12ч * 3600с
        // Новое время "намного больше" → вероятно, переход назад через полночь
        delta_sec -= 86400;
        day_shift = -1;
    } else if (delta_sec < -43200) {
        // Новое время "намного меньше" → переход вперёд через полночь
        delta_sec += 86400;
        day_shift = +1;
    }

    // Копируем текущую дату из RTC
    uint8_t day   = bcd2bin(time.date, time.date_10);
    uint8_t month = bcd2bin(time.month, time.month_10);
    uint8_t year  = bcd2bin(time.year, time.year_10);

    // Применяем сдвиг даты, если он необходим
    if (day_shift == +1) {
        date_inc(&day, &month, &year);
    } else if (day_shift == -1) {
        date_dec(&day, &month, &year);
    }

    // Собираем новую структуру DS3231_t, сохранив остальные поля (например, cfg)
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

    // Вычисляем абсолютное время ДО и ПОСЛЕ коррекции
    uint32_t old_abs = DS3231_date_to_sec(&time);
    uint32_t new_abs = DS3231_date_to_sec(&time_new);

    // Коррекция частоты (AGING) через регистр 0x10
    // Условия: ошибка не больше 30 минут, прошло больше 5 минут с последней корр.
    if (delta_sec >= -1800 && delta_sec <= 1800 && (old_abs - *last_correct_sec) >= 300) {
        // Рассчитываем ppm * 2^15 (около 32768) для коррекции 1ppm
        // Результат в диапазоне -128..127 для 8-битного регистра AGING
        int64_t pps = (int64_t)delta_sec * 10000000LL / (int64_t)(old_abs - *last_correct_sec);
        if (pps >= -128 && pps <= 127) {
            time_new.aging_offset = (uint8_t)(int8_t)pps;
        }
    }

    // Установить бит запуска термокомпенсации (если используется)
    time_new.conv = 1;

    res = DS3231_Write(&time_new);
    if (res > 0) {
        // Обновить время последней коррекции только при успехе
        *last_correct_sec = new_abs;
    }
    return res;
}