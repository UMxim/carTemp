/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    main.c
  * @author  Макс
  * @brief   Автомобильный информационный дисплей на STM32L011
  *
  * @details
  * Устройство предназначено для установки в автомобиль и отображает:
  *   - Точное время с модуля DS3231 (RTC) на OLED-дисплее SSD1306.
  *   - Напряжение АКБ с предупреждением при выходе за допустимые пределы.
  *   - Температуру двигателя с датчика DS1621.
  *   - Автоматически регулирует яркость дисплея в зависимости от:
  *       • наличия зажигания (Veng),
  *       • состояния фар (Vlight).
  *
  * Управление:
  *   - Три кнопки: левая (H), средняя (M), правая (R).
  *   - Длинное нажатие правой кнопки → вход в режим настройки времени.
  *   - В режиме настройки:
  *       • Левая кнопка — увеличение часов,
  *       • Средняя — увеличение минут,
  *       • Короткое нажатие правой — сохранение и выход.
  *
  * Архитектура:
  *   - Без RTOS, кооперативная многозадачность через программные таймеры.
  *   - Все циклы (_cycle) вызываются в main() и работают независимо.
  *   - Защита от зависания — IWDG (независимый сторожевой таймер).
  *   - АЦП: 4 канала, медианная фильтрация по 8 выборкам, калибровка по VREFINT.
  *
  * Подключение:
  *   - Кнопки: PC14 (H), PC15 (M), PA7 (R) — активный уровень: низкий.
  *   - АЦП: канал 1 — Veng, канал 0 — Vbat, канал 4 — Vlight.
  *   - I2C1: SCL — PB6, SDA — PB7 (DS3231 и DS1621).
  *   - OLED: SSD1306 по I2C на том же шине.
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
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "misc.h"
#include "ssd1306.h"
#include "rtc_DS3231.h"
#include "ds1621.h"
#include "stm32l011_my_hal.h"
/* USER CODE END Includes */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

// -------------------------------------------------------------------------
// ПЕРИОДЫ ОБНОВЛЕНИЯ (в миллисекундах)
// -------------------------------------------------------------------------
#define CLOCK_UPDATE_PERIOD_MS          500     ///< Обновление времени дважды в секунду (для мигания двоеточия)
#define VOLTAGE_UPDATE_PERIOD_MS        200     ///< Обновление напряжения АКБ 5 раз/сек
#define TEMPERATURE_UPDATE_PERIOD_MS    1000    ///< Чтение температуры раз в секунду
#define BUTTON_UPDATE_PERIOD_MS         10      ///< Опрос кнопок каждые 10 мс (антидребезг)
#define ADC_UPDATE_PERIOD_MS            100     ///< Полный цикл АЦП (включая усреднение по 8 точкам)
#define SCREEN_UPDATE_PERIOD_MS         100     ///< Обновление OLED раз в 100 мс

// -------------------------------------------------------------------------
// ПАРАМЕТРЫ ДЕЛИТЕЛЕЙ НАПРЯЖЕНИЯ (в омах)
// Используются для расчёта реального напряжения по показаниям АЦП:
//   V_реал = V_АЦП * (R_HI + R_LO) / R_LO
// -------------------------------------------------------------------------
#define R_HI_V_ENG                      83200   ///< Верхний резистор делителя для напряжения зажигания
#define R_LO_V_ENG                      19860   ///< Нижний резистор делителя для напряжения зажигания
#define R_HI_V_BAT                      82400   ///< Верхний резистор делителя для напряжения АКБ
#define R_LO_V_BAT                      19890   ///< Нижний резистор делителя для напряжения АКБ
#define R_HI_V_LIGHT                    81800   ///< Верхний резистор делителя для напряжения фар
#define R_LO_V_LIGHT                    19940   ///< Нижний резистор делителя для напряжения фар

// -------------------------------------------------------------------------
// ПОРОГИ ПРЕДУПРЕЖДЕНИЙ (в милливольтах и градусах Цельсия)
// -------------------------------------------------------------------------
#define V_BAT_HI_WARNING_mV             14600   ///< Предупреждение: напряжение АКБ > 14.6 В (перезаряд)
#define V_BAT_LO_WARNING_mV             12000   ///< Предупреждение: напряжение АКБ < 12.0 В (разряд)
#define V_LO_THRESHOLD_mV               2000    ///< Порог: зажигание считается выключенным (<2 В)
#define V_LO_THRESHOLD_LIGHT_mV         8000    ///< Порог: фары выключены (<8 В) → днём → макс. яркость
#define T_HI_WARNING                    106     ///< Предупреждение: температура >= 106°C (перегрев)
#define T_LO_WARNING                    0       ///< Предупреждение: температура <= 0°C

// -------------------------------------------------------------------------
// КАЛИБРОВКА АЦП
// Эмпирические коэффициенты для коррекции систематической ошибки измерений.
// Формула: V_корр = ((V_расч * VOLTAGE_CORRECT_K) >> 16) + VOLTAGE_CORRECT_B
// -------------------------------------------------------------------------
#define VOLTAGE_CORRECT_K               67650   ///< Множитель в формате Q16.16 (сдвиг на 16 бит)
#define VOLTAGE_CORRECT_B               -159    ///< Аддитивная поправка, мВ

// -------------------------------------------------------------------------
// ПАРАМЕТРЫ ОБРАБОТКИ КНОПОК
// -------------------------------------------------------------------------
#define ADC_AVRG_NUM                    8       ///< Количество выборок АЦП для медианной фильтрации
#define BUTTON_LONG_PRESS_COUNT_LIMIT   (10000 / BUTTON_UPDATE_PERIOD_MS)  ///< 1000 = 10 сек (длинное нажатие)
#define BUTTON_PRESS_COUNT_LIMIT        (100 / BUTTON_UPDATE_PERIOD_MS)    ///< 10 = 100 мс (короткое нажатие)

/* USER CODE END PD */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/**
 * @brief Состояния кнопок
 */
enum button_state {
    BUTTON_IDLE = 0,   ///< Кнопка не нажата
    BUTTON_PRESS,      ///< Короткое нажатие (100 мс <= t < 10 сек)
    BUTTON_LPRESS      ///< Длинное нажатие (t >= 10 сек)
};

/**
 * @brief Имена кнопок (соответствуют физическим кнопкам)
 */
enum button_name {
    BUTTON_H = 0,      ///< Левая кнопка (High / Hours)
    BUTTON_M,          ///< Средняя кнопка (Minutes)
    BUTTON_R           ///< Правая кнопка (Right / Enter)
};

/**
 * @brief Глобальный кэш состояния системы
 *
 * Все данные, которые должны сохраняться между вызовами _cycle() функций,
 * хранятся здесь. Это позволяет избежать глобальных переменных в разброс.
 */
struct {
    // --- Время ---
    DS3231_t time;                  ///< Текущее время от модуля DS3231
    timer_t tim_update_clock;       ///< Таймер обновления часов

    // --- Напряжения ---
    timer_t tim_update_voltage;     ///< Таймер обновления отображения напряжения
    timer_t tim_update_adc;         ///< Таймер АЦП
    uint32_t Veng_mV;               ///< Напряжение зажигания (мВ)
    uint32_t Vbat_mV;               ///< Напряжение АКБ (мВ)
    uint32_t Vlight_mV;             ///< Напряжение фар (мВ)

    // --- Температура ---
    timer_t tim_update_temperature; ///< Таймер обновления температуры

    // --- Кнопки ---
    timer_t tim_update_button;      ///< Таймер опроса кнопок
    enum button_state button_state[BUTTON_R + 1]; ///< Текущее состояние трёх кнопок

    // --- Дисплей ---
    timer_t tim_update_screen;      ///< Таймер обновления OLED
} cache;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
 * @brief Режим редактирования времени
 *
 * Управление:
 *   - Длинное нажатие правой кнопки (BUTTON_R) → вход в режим настройки.
 *   - В режиме настройки:
 *       • Левая кнопка (BUTTON_H) — +1 час (0–23),
 *       • Средняя кнопка (BUTTON_M) — +1 минута (0–59),
 *       • Короткое нажатие правой кнопки — сохранить и выйти.
 *
 * Особенности:
 *   - При изменении времени строка немедленно отображается на экране (инвертированная).
 *   - После выхода из режима все кнопки сбрасываются в BUTTON_IDLE.
 *   - Время корректируется через DS3231_correct(), метка коррекции сохраняется в EEPROM[0].
 *
 * @return 1, если находится в режиме редактирования; 0 — в обычном режиме.
 */
int Clock_edit(void)
{
    static uint8_t state = 0;           // 0 — ожидание входа, 1 — редактирование
    static uint8_t new_hour = 0;        // Редактируемое значение часов
    static uint8_t new_minutes = 0;     // Редактируемое значение минут
    static uint8_t need_update = 0;     // Флаг необходимости обновить отображение

    switch (state) {
        case 0:
            // Ожидаем длинное нажатие правой кнопки для входа в настройку
            if (cache.button_state[BUTTON_R] == BUTTON_LPRESS) {
                // Загружаем текущее время из кэша
                new_hour = cache.time.hours_10 * 10 + cache.time.hours;
                new_minutes = cache.time.minutes_10 * 10 + cache.time.minutes;
                state = 1;
                need_update = 1;
            }
            break;

        case 1:
            // Управление часами
            if (cache.button_state[BUTTON_H] == BUTTON_PRESS) {
                if (new_hour < 23) new_hour++; else new_hour = 0;
                need_update = 1;
            }

            // Управление минутами
            if (cache.button_state[BUTTON_M] == BUTTON_PRESS) {
                if (new_minutes < 59) new_minutes++; else new_minutes = 0;
                need_update = 1;
            }

            // Сохранение и выход
            if (cache.button_state[BUTTON_R] == BUTTON_PRESS) {
                uint32_t last_correct_sec = EEPROM_ReadWord(0); // Читаем метку из EEPROM
                int res = DS3231_correct(new_hour, new_minutes, 0, &last_correct_sec);
                if (res > 0) EEPROM_WriteWord(0, last_correct_sec); // Сохраняем новую метку
                res = DS3231_Read(&cache.time); // Обновляем кэш времени
                state = 0;
            }

            // Обновление отображения редактируемого времени
            if (need_update) {
                static char time_str[6] = {[5] = 0}; // "HH MM\0"
                time_str[0] = '0' + new_hour / 10;
                time_str[1] = '0' + new_hour % 10;
                time_str[2] = ' ';
                time_str[3] = '0' + new_minutes / 10;
                time_str[4] = '0' + new_minutes % 10;
                ssd1306_SetCursor(0, 0);
                ssd1306_WriteString(time_str, 1, 1); // Инвертированный текст
                need_update = 0;
            }
            break;

        default:
            state = 0;
            break;
    }

    // Сброс состояния кнопок после обработки (защита от повторного срабатывания)
    cache.button_state[BUTTON_R] = BUTTON_IDLE;
    cache.button_state[BUTTON_H] = BUTTON_IDLE;
    cache.button_state[BUTTON_M] = BUTTON_IDLE;

    return state;
}

/**
 * @brief Цикл обновления часов и отображения времени
 *
 * Логика:
 *   - При первом вызове инициализирует таймер обновления.
 *   - Если активен режим редактирования (Clock_edit() == 1) — обновление приостанавливается.
 *   - Каждые CLOCK_UPDATE_PERIOD_MS (500 мс):
 *       • Через раз читает время из DS3231 (чтобы не перегружать шину),
 *       • Формирует строку "HH:MM" с мигающим двоеточием (через раз — пробел),
 *       • При ошибке чтения выводит "::X::", где X = -код ошибки.
 */
void Clock_cycle(void)
{
    static uint8_t cnt = 0;
    static char time_str[6] = {[5] = 0}; // "HH:MM\0"
    static uint8_t is_init = 0;

    if (!is_init) {
        Timer_set(&cache.tim_update_clock, CLOCK_UPDATE_PERIOD_MS);
        is_init = 1;
    }

    int is_clock_edit = Clock_edit(); // Обязательно вызывать каждый цикл!
    if (Timer_isExpired(&cache.tim_update_clock)) {
        if (is_clock_edit) return; // В режиме редактирования не обновляем время

        cnt++;
        static int res = 0;
        if (cnt & 1) {
            res = DS3231_Read(&cache.time); // Читаем только на нечётных тиках
        }

        if (res > 0) {
            // Успешное чтение — формируем строку времени
            time_str[0] = '0' + cache.time.hours_10;
            time_str[1] = '0' + cache.time.hours;
            time_str[2] = (cnt & 1) ? ' ' : ':'; // Мигание двоеточия
            time_str[3] = '0' + cache.time.minutes_10;
            time_str[4] = '0' + cache.time.minutes;
        } else {
            // Ошибка чтения — выводим диагностический код
            time_str[0] = ':';
            time_str[1] = ':';
            time_str[2] = '0' - res; // Например, если res = -2 → '2'
            time_str[3] = ':';
            time_str[4] = ':';
        }

        ssd1306_SetCursor(0, 0);
        ssd1306_WriteString(time_str, 1, 0); // Нормальный (не инвертированный) текст
    }
}

/**
 * @brief Цикл опроса кнопок с антидребезгом
 *
 * Подключение:
 *   - BUTTON_H (левая): PC14, активный уровень — низкий.
 *   - BUTTON_M (средняя): PC15, активный уровень — низкий.
 *   - BUTTON_R (правая): PA7, активный уровень — низкий.
 *
 * Логика:
 *   - Каждые BUTTON_UPDATE_PERIOD_MS (10 мс) опрашивает состояние GPIO.
 *   - Для каждой кнопки ведёт счётчик непрерывного нажатия.
 *   - При отпускании:
 *       • Если счётчик в [BUTTON_PRESS_COUNT_LIMIT, BUTTON_LONG_PRESS_COUNT_LIMIT) → BUTTON_PRESS,
 *       • Если счётчик >= BUTTON_LONG_PRESS_COUNT_LIMIT → BUTTON_LPRESS.
 *   - Счётчик сбрасывается при отпускании.
 */
void Button_cycle(void)
{
    static uint8_t is_init = 0;
    if (!is_init) {
        Timer_set(&cache.tim_update_button, BUTTON_UPDATE_PERIOD_MS);
        is_init = 1;
    }

    static uint32_t button_counter[BUTTON_R + 1] = {0};
    uint8_t button_press[BUTTON_R + 1];

    if (Timer_isExpired(&cache.tim_update_button)) {
        // Опрос GPIO (активный уровень — низкий)
        button_press[BUTTON_H] = !LL_GPIO_IsInputPinSet(GPIOC, LL_GPIO_PIN_14);
        button_press[BUTTON_M] = !LL_GPIO_IsInputPinSet(GPIOC, LL_GPIO_PIN_15);
        button_press[BUTTON_R] = !LL_GPIO_IsInputPinSet(GPIOA, LL_GPIO_PIN_7);

        for (int i = BUTTON_H; i <= BUTTON_R; i++) {
            if (button_press[i]) {
                button_counter[i]++;
            } else {
                // Кнопка отпущена — анализируем длительность нажатия
                if ((button_counter[i] > BUTTON_PRESS_COUNT_LIMIT) &&
                    (button_counter[i] < BUTTON_LONG_PRESS_COUNT_LIMIT)) {
                    cache.button_state[i] = BUTTON_PRESS;
                }
                button_counter[i] = 0; // Сброс счётчика
            }
            // Проверка на длинное нажатие (даже если кнопка всё ещё нажата)
            if (button_counter[i] >= BUTTON_LONG_PRESS_COUNT_LIMIT) {
                cache.button_state[i] = BUTTON_LPRESS;
            }
        }
    }
}

/**
 * @brief Цикл управления OLED-дисплеем SSD1306
 *
 * Логика регулировки яркости:
 *   1. Если Veng < V_LO_THRESHOLD_mV (2 В) → зажигание выключено → яркость = 0 (минимум).
 *   2. Если Vlight < V_LO_THRESHOLD_LIGHT_mV (8 В) → фары выключены → считаем, что днём → яркость = 0xFF (максимум).
 *   3. Если Vlight > Veng → ошибка подключения → яркость = 0.
 *   4. Иначе (фары включены, зажигание включено):
 *        яркость = 0xFF - (Vlight - 8000) * 0xFF / (Veng - 8000)
 *        → чем выше Vlight (ближе к Veng), тем темнее экран.
 *
 * Дополнительно:
 *   - При первом вызове инициализирует дисплей.
 *   - Обновляет экран каждые SCREEN_UPDATE_PERIOD_MS.
 */
void Display_cycle(void)
{
    static uint8_t is_init = 0;
    if (!is_init) {
        ssd1306_Init(); // Инициализация OLED
        Timer_set(&cache.tim_update_screen, SCREEN_UPDATE_PERIOD_MS);
        is_init = 1;
    }

    if (!Timer_isExpired(&cache.tim_update_screen)) return;

    static uint8_t light = 1; // Текущая яркость (сохраняется между вызовами)
    uint8_t new_light = 0;

    // Расчёт новой яркости
    do {
        // Зажигание выключено
        if (cache.Veng_mV < V_LO_THRESHOLD_mV) {
            new_light = 0;
            break;
        }

        // Фары выключены → днём → макс. яркость
        if (cache.Vlight_mV < V_LO_THRESHOLD_LIGHT_mV) {
            new_light = 0xFF;
            break;
        }

        // Защита от некорректных данных
        if (cache.Vlight_mV > cache.Veng_mV) {
            new_light = 0;
            break;
        }

        // Фары включены → регулируем яркость по напряжению
        // Линейная зависимость от 8 В (макс. яркость) до Veng (мин. яркость)
        uint32_t numerator = (cache.Vlight_mV - V_LO_THRESHOLD_LIGHT_mV) * 0xFF;
        uint32_t denominator = (cache.Veng_mV - V_LO_THRESHOLD_LIGHT_mV);
        if (denominator == 0) {
            new_light = 0xFF; // На всякий случай
        } else {
            new_light = 0xFF - (uint8_t)(numerator / denominator);
        }
    } while (0);

    // Применяем новую яркость
    ssd1306_SetContrast(light);
    light = new_light;

    // Обновляем содержимое экрана
    ssd1306_UpdateScreen();
}

/**
 * @brief Цикл измерения аналоговых напряжений
 *
 * Измеряемые каналы:
 *   - Veng: LL_ADC_CHANNEL_1 → напряжение зажигания
 *   - Vbat: LL_ADC_CHANNEL_0 → напряжение АКБ
 *   - Vlight: LL_ADC_CHANNEL_4 → напряжение фар
 *   - Vref: LL_ADC_CHANNEL_VREFINT → внутренний опорный для калибровки
 *
 * Логика:
 *   - Каждые (ADC_UPDATE_PERIOD_MS / ADC_AVRG_NUM) = 12.5 мс измеряется один канал.
 *   - После 8 измерений (полный цикл) выполняется обработка:
 *       1. Вычисляется медиана по Vrefint → определяется коэффициент масштаба k.
 *       2. Для каждого канала:
 *           - Берётся медиана из 8 значений,
 *           - Переводится в мВ: V_mV = raw * (VREFINT_CAL / Vref_median),
 *           - Умножается на коэффициент делителя,
 *           - Применяется линейная коррекция (K и B).
 */
void ADC_cycle(void)
{
    enum channels_name { Veng = 0, Vbat, Vlight, Vref, size_ };
    const uint32_t channels_[size_] = {
        LL_ADC_CHANNEL_1,           // Veng
        LL_ADC_CHANNEL_0,           // Vbat
        LL_ADC_CHANNEL_4,           // Vlight
        LL_ADC_CHANNEL_VREFINT      // Vref
    };

    static uint16_t adc[size_][ADC_AVRG_NUM] = {0}; // Кольцевой буфер измерений
    static uint8_t curr = 0;                        // Текущий индекс в буфере

    static uint8_t is_init = 0;
    if (!is_init) {
        Timer_set(&cache.tim_update_adc, ADC_UPDATE_PERIOD_MS / ADC_AVRG_NUM);
        is_init = 1;
    }

    if (!Timer_isExpired(&cache.tim_update_adc)) return;

    // Выполняем одно измерение по каждому каналу
    for (int i = Veng; i < size_; i++) {
        adc[i][curr] = Read_ADC_Channel(channels_[i]);
    }

    curr++;
    if (curr == ADC_AVRG_NUM) { // Собрали полный набор → обрабатываем
        curr = 0;

        // Калибровка по внутреннему опорному напряжению
        uint16_t V_ref_median = GetMedian_16(&adc[Vref][0], ADC_AVRG_NUM);
        uint32_t k = GET_ADC_K(ADC_V_REF_mV, V_ref_median); // k = (Vref_cal * 4095) / V_ref_median

        // Обработка Veng
        uint16_t V = GetMedian_16(&adc[Veng][0], ADC_AVRG_NUM);
        cache.Veng_mV = GET_mV(V, k) * (R_HI_V_ENG + R_LO_V_ENG) / R_LO_V_ENG;
        cache.Veng_mV = ((VOLTAGE_CORRECT_K * cache.Veng_mV) >> 16) + VOLTAGE_CORRECT_B;

        // Обработка Vbat
        V = GetMedian_16(&adc[Vbat][0], ADC_AVRG_NUM);
        cache.Vbat_mV = GET_mV(V, k) * (R_HI_V_BAT + R_LO_V_BAT) / R_LO_V_BAT;
        cache.Vbat_mV = ((VOLTAGE_CORRECT_K * cache.Vbat_mV) >> 16) + VOLTAGE_CORRECT_B;

        // Обработка Vlight
        V = GetMedian_16(&adc[Vlight][0], ADC_AVRG_NUM);
        cache.Vlight_mV = GET_mV(V, k) * (R_HI_V_LIGHT + R_LO_V_LIGHT) / R_LO_V_LIGHT;
        cache.Vlight_mV = ((VOLTAGE_CORRECT_K * cache.Vlight_mV) >> 16) + VOLTAGE_CORRECT_B;
    }
}

/**
 * @brief Отображение напряжения АКБ на OLED
 *
 * Логика:
 *   - Каждые VOLTAGE_UPDATE_PERIOD_MS (200 мс) обновляет строку вида " 12.5v".
 *   - Если Vbat_mV выходит за [V_BAT_LO_WARNING_mV, V_BAT_HI_WARNING_mV] → текст инвертируется.
 *   - Позиция: правый верхний угол (80, 0).
 */
void Voltage_cycle(void)
{
    static uint8_t is_init = 0;
    if (!is_init) {
        Timer_set(&cache.tim_update_voltage, VOLTAGE_UPDATE_PERIOD_MS);
        is_init = 1;
    }

    if (!Timer_isExpired(&cache.tim_update_voltage)) return;

    // Проверка на аварийное напряжение
    uint8_t is_warning = (cache.Vbat_mV < V_BAT_LO_WARNING_mV) ||
                         (cache.Vbat_mV > V_BAT_HI_WARNING_mV);

    // Преобразуем Vbat_mV в строку (например, 12500 → "      12500")
    char v_mV[12];
    Int_to_str(cache.Vbat_mV, v_mV);

    // Формируем строку " 12.5v" (пробелы для выравнивания до 6 символов)
    char v[7] = " 12.5v";
    v[1] = v_mV[6]; // Десятки вольт 
    v[2] = v_mV[7]; // Единицы вольт
    v[4] = v_mV[8]; // десятые вольта (после точки)

    ssd1306_SetCursor(80, 0);
    ssd1306_WriteString(v, 0, is_warning); // is_warning → инверсия
}

/**
 * @brief Цикл измерения и отображения температуры
 *
 * Логика:
 *   - При первом вызове инициализирует DS1621 и запускает первое преобразование.
 *   - Каждую секунду:
 *       • Читает результат предыдущего преобразования,
 *       • Сразу запускает следующее (DS1621 работает асинхронно),
 *       • Отображает температуру в формате "  -33c",
 *       • При выходе за [T_LO_WARNING, T_HI_WARNING] → инверсия текста.
 *   - Позиция: правый нижний угол (80, 16).
 */
void Temperature_cycle(void)
{
    static uint8_t is_init = 0;
    if (!is_init) {
        Timer_set(&cache.tim_update_temperature, TEMPERATURE_UPDATE_PERIOD_MS);
        ds1621_cfg_t cfg = {0};
        DS1621_set_cfg(&cfg);       // Сбрасываем конфигурацию датчика
        Timer_delay_ms(10);
        DS1621_start_convert();     // Запускаем первое преобразование
        is_init = 1;
    }

    if (!Timer_isExpired(&cache.tim_update_temperature)) return;

    ds1621_temp_t temp = DS1621_get_temp(); // Читаем результат
    DS1621_start_convert();                 // Запускаем следующее преобразование

    // Проверка на аварийную температуру
    uint8_t is_warning = (temp.temp <= T_LO_WARNING) || (temp.temp >= T_HI_WARNING);

    // Преобразуем температуру в строку (например, -33 → "      -33")
    char str_temp[12];
    Int_to_str(temp.temp, str_temp);

    // Формируем строку "  -33c"
    char disp[7] = "  -33c";
    disp[2] = str_temp[8]; // Сотни (обычно ' ' или '-')
    disp[3] = str_temp[9]; // Десятки
    disp[4] = str_temp[10]; // Единицы

    ssd1306_SetCursor(80, 16);
    ssd1306_WriteString(disp, 0, is_warning);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* MCU Configuration */
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
  NVIC_SetPriority(SysTick_IRQn, 3);

  /* System Clock */
  SystemClock_Config();

  /* Peripheral Initialization */
  MX_GPIO_Init();
  MX_ADC_Init();
  MX_I2C1_Init();

  /* USER CODE BEGIN 2 */
  timer_ms_init();              // Инициализация программных таймеров
  IWDG_Start_MaxTimeout();      // Запуск сторожевого таймера (~32 сек)
  /* USER CODE END 2 */

  /* Infinite loop */
  while (1) {
    Clock_cycle();              // Обновление времени
    Button_cycle();             // Опрос кнопок
    ADC_cycle();                // Измерение напряжений
    Voltage_cycle();            // Отображение напряжения АКБ
    Temperature_cycle();        // Измерение и отображение температуры
    Display_cycle();            // Обновление OLED и яркости

    IWDG_Refresh();             // Сброс сторожевого таймера
  }
}

/**
  * @brief System Clock Configuration
  * @details
  * Используется внутренний HSI (16 МГц) без PLL.
  * Тактирование:
  *   - SYSCLK = 16 МГц,
  *   - AHB = 16 МГц,
  *   - APB1/APB2 = 16 МГц.
  * Источник тактирования I2C1 — PCLK1.
  */
void SystemClock_Config(void)
{
  LL_FLASH_SetLatency(LL_FLASH_LATENCY_0);
  while(LL_FLASH_GetLatency() != LL_FLASH_LATENCY_0) {}

  LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);
  while (LL_PWR_IsActiveFlag_VOS() != 0) {}

  LL_RCC_HSI_Enable();
  while(LL_RCC_HSI_IsReady() != 1) {}
  LL_RCC_HSI_SetCalibTrimming(16);

  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
  LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_1);
  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSI);
  while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSI) {}

  LL_Init1msTick(16000000);
  LL_SetSystemCoreClock(16000000);
  LL_RCC_SetI2CClockSource(LL_RCC_I2C1_CLKSOURCE_PCLK1);
}

/* USER CODE END 4 */

/**
  * @brief  Error handler
  * @details
  * При критической ошибке отключает прерывания и зависает в бесконечном цикле.
  */
void Error_Handler(void)
{
  __disable_irq();
  while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  // Можно добавить отладочный вывод, если нужно
}
#endif /* USE_FULL_ASSERT */