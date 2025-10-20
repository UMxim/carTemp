/**
 * @file ssd1306.c
 * @brief Реализация библиотеки для работы с OLED-дисплеями SSD1306.
 *
 * Реализует функции из заголовочного файла ssd1306.h.
 * Содержит логику для инициализации дисплея, работы с внутренним буфером
 * изображения и отправки данных на дисплей.
 * Использует пользовательские функции I2C/SPI (в данном случае i2c_write).
 *
 * Логика работы:
 * - ssd1306_Init: Посылает команды для настройки контроллера SSD1306,
 *   очищает буфер и обновляет экран.
 * - Рисование: Функции (DrawPixel, Line, Circle и т.д.) изменяют биты
 *   в глобальном буфере SSD1306_Buffer.
 * - ssd1306_UpdateScreen: Посылает содержимое SSD1306_Buffer на дисплей
 *   постранично.
 * - Низкий уровень: ssd1306_WriteCommand/ssd1306_WriteData используют
 *   пользовательские I2C/SPI функции для обмена с дисплеем.
 */

#include "ssd1306.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>  // Для memcpy


// ========== Конфигурация пользователя ==========
// В .h нельзя переносить. Там массив большой и статик. Везде будет выделяться
#include "font_spleen_8x16.h"
#include "font_spleen_16x32.h"
#include "stm32l011_my_hal.h"
//static const font_descriptor_t * const font_descr[] = {&font_ter_u12b, &font_spleen_12x24};
/// @brief Массив указателей на дескрипторы используемых шрифтов.
static const font_descriptor_t * const font_descr[] = {&font_spleen_8x16, &font_spleen_16x32};

/**
 * @brief Низкоуровневая функция отправки данных на дисплей по I2C.
 * @param[in] reg Регистр данных (0x40) или команд (0x00).
 * @param[in] buffer Указатель на буфер с данными.
 * @param[in] buff_size Размер буфера данных.
 */
void ssd1306_WriteData(uint8_t reg, uint8_t* buffer, size_t buff_size)
{
	// Предполагается, что i2c_write реализована пользователем
	// и обрабатывает протокол I2C для SSD1306 (Write followed by Data).
	// reg передаётся как первый байт данных.
	i2c_write(I2C1, SSD1306_I2C_ADDR, &reg, 1, buffer, buff_size);
}
// ========== -Конфигурация пользователя ==========

/// @brief Внутренний буфер изображения размером SSD1306_BUFFER_SIZE байт.
static uint8_t SSD1306_Buffer[SSD1306_BUFFER_SIZE];

/// @brief Структура для хранения состояния дисплея.
static struct
{
    uint16_t CurrentX;      ///< Текущая X координата курсора.
    uint16_t CurrentY;      ///< Текущая Y координата курсора.
    uint8_t Initialized;    ///< Флаг инициализации.
    uint8_t DisplayOn;      ///< Флаг включения дисплея.
} SSD1306;

/**
 * @brief Отправка команды дисплею.
 * @param[in] byte Байт команды.
 */
void ssd1306_WriteCommand(uint8_t byte)
{
	// Команда передаётся через тот же интерфейс, что и данные,
	// но с префиксом 0x00 (Co=1, D/C#=0).
	ssd1306_WriteData(0x00, &byte, 1);
}

/**
 * @brief Заполнение внутреннего буфера изображения данными.
 * @param[in] buf Указатель на буфер с данными.
 * @param[in] len Количество байт для копирования.
 * @return 1 при успехе, -1 при превышении размера буфера.
 */
int ssd1306_FillBuffer(uint8_t* buf, uint32_t len)
{
    int ret = -1;
    if (len <= SSD1306_BUFFER_SIZE)
    {
        memcpy(SSD1306_Buffer,buf,len);
        ret = 1;
    }
    return ret;
}

/**
 * @brief Инициализация дисплея SSD1306.
 *
 * Отправляет последовательность команд для настройки контроллера SSD1306
 * в соответствии с параметрами из заголовочного файла.
 * Также очищает буфер и обновляет экран.
 */
void ssd1306_Init()
{
    // Ждём, пока дисплей загрузится
	Timer_delay_ms(100);

    // Инициализация OLED
    ssd1306_SetDisplayOn(0); // дисплей выключен

    ssd1306_WriteCommand(0x20); // Установить режим адресации памяти
    ssd1306_WriteCommand(0x00); // 00b - Горизонтальный режим; 01b - Вертикальный;
                                // 10b - Режим страниц (RESET); 11b - Неверный

    ssd1306_WriteCommand(0xB0); // Установить адрес страницы начала (для режима страниц, 0-7)

#ifdef SSD1306_MIRROR_VERT
    ssd1306_WriteCommand(0xC0); // Отразить вертикально
#else
    ssd1306_WriteCommand(0xC8); // Установить направление сканирования COM
#endif

    ssd1306_WriteCommand(0x00); // Установить низкий адрес колонки
    ssd1306_WriteCommand(0x10); // Установить высокий адрес колонки

    ssd1306_WriteCommand(0x40); // Установить адрес стартовой линии - ПРОВЕРИТЬ

    ssd1306_SetContrast(0xFF);

#ifdef SSD1306_MIRROR_HORIZ
    ssd1306_WriteCommand(0xA0); // Отразить горизонтально
#else
    ssd1306_WriteCommand(0xA1); // Установить сопоставление сегментов 0-127 - ПРОВЕРИТЬ
#endif

#ifdef SSD1306_INVERSE_COLOR
    ssd1306_WriteCommand(0xA7); // Установить инвертированный цвет
#else
    ssd1306_WriteCommand(0xA6); // Установить нормальный цвет
#endif

// Установить коэффициент мультиплексирования.
#if (SSD1306_HEIGHT == 128)
    // Найдено в Python библиотеке Luma для SH1106.
    ssd1306_WriteCommand(0xFF);
#else
    ssd1306_WriteCommand(0xA8); // Установить коэффициент мультиплексирования (1 to 64) - ПРОВЕРИТЬ
#endif

#if (SSD1306_HEIGHT == 32)
    ssd1306_WriteCommand(0x1F); //
#elif (SSD1306_HEIGHT == 64)
    ssd1306_WriteCommand(0x3F); //
#elif (SSD1306_HEIGHT == 128)
    ssd1306_WriteCommand(0x3F); // Работает и для дисплеев высотой 128px.
#else
#error "Поддерживаются только 32, 64 или 128 строк высоты!"
#endif

    ssd1306_WriteCommand(0xA4); // 0xa4 - Вывод следует содержимому RAM; 0xa5 - Вывод игнорирует содержимое RAM

    ssd1306_WriteCommand(0xD3); // Установить смещение дисплея - ПРОВЕРИТЬ
    ssd1306_WriteCommand(0x00); // Без смещения

    ssd1306_WriteCommand(0xD5); // Установить коэффициент деления частоты дисплея / частоту осциллятора
    ssd1306_WriteCommand(0xF0); // Установить коэффициент деления

    ssd1306_WriteCommand(0xD9); // Установить период предварительной зарядки
    ssd1306_WriteCommand(0x22); //

    ssd1306_WriteCommand(0xDA); // Установить аппаратную конфигурацию выводов COM - ПРОВЕРИТЬ
#if (SSD1306_HEIGHT == 32)
    ssd1306_WriteCommand(0x02);
#elif (SSD1306_HEIGHT == 64)
    ssd1306_WriteCommand(0x12);
#elif (SSD1306_HEIGHT == 128)
    ssd1306_WriteCommand(0x12);
#else
#error "Поддерживаются только 32, 64 или 128 строк высоты!"
#endif

    ssd1306_WriteCommand(0xDB); // Установить vcomh
    ssd1306_WriteCommand(0x20); // 0x20 - 0.77xVcc

    ssd1306_WriteCommand(0x8D); // Установить включение DC-DC
    ssd1306_WriteCommand(0x14); //
    ssd1306_SetDisplayOn(1); // Включить панель SSD1306

    // Очистить экран
    ssd1306_Fill(SSD1306_COLOR_BLACK);
    
    // Отправить буфер на экран
    ssd1306_UpdateScreen();
    
    // Установить значения по умолчанию для объекта экрана
    SSD1306.CurrentX = 0;
    SSD1306.CurrentY = 0;
    
    SSD1306.Initialized = 1;
}

/**
 * @brief Заполнение всего экрана заданным цветом.
 * @param[in] color Цвет для заливки (SSD1306_COLOR_WHITE или SSD1306_COLOR_BLACK).
 */
void ssd1306_Fill(SSD1306_COLOR color)
{
    // Заполнить буфер байтами 0x00 (чёрный) или 0xFF (белый)
    memset(SSD1306_Buffer, (color == SSD1306_COLOR_BLACK) ? 0x00 : 0xFF, sizeof(SSD1306_Buffer));
}

/**
 * @brief Отправка содержимого буфера на физический дисплей.
 *
 * Данные отправляются постранично. Количество страниц зависит от высоты дисплея.
 */
void ssd1306_UpdateScreen(void)
{
    // Запись данных в каждую страницу RAM. Количество страниц
    // зависит от высоты экрана:
    //
    //  * 32px   ==  4 страницы
    //  * 64px   ==  8 страниц
    //  * 128px  ==  16 страниц
    for(uint8_t i = 0; i < SSD1306_HEIGHT/8; i++)
    {
        ssd1306_WriteCommand(0xB0 + i); // Установить текущий адрес страницы RAM.
        ssd1306_WriteCommand(0x00 + SSD1306_X_OFFSET_LOWER);
        ssd1306_WriteCommand(0x10 + SSD1306_X_OFFSET_UPPER);
        // Отправить 128 байт данных для текущей страницы
        ssd1306_WriteData(0x40, &SSD1306_Buffer[SSD1306_WIDTH*i],SSD1306_WIDTH);
    }
}

/**
 * @brief Рисование одного пикселя в буфере.
 * @param[in] x X Координата.
 * @param[in] y Y Координата.
 * @param[in] color Цвет пикселя.
 */
void ssd1306_DrawPixel(uint8_t x, uint8_t y, SSD1306_COLOR color)
{
    // Проверка границ
    if(x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT) return;

    // Рисование нужным цветом
    if(color == SSD1306_COLOR_WHITE)
    {
        // Установить соответствующий бит
        SSD1306_Buffer[x + (y / 8) * SSD1306_WIDTH] |= 1 << (y % 8);
    } else
    {
        // Сбросить соответствующий бит
        SSD1306_Buffer[x + (y / 8) * SSD1306_WIDTH] &= ~(1 << (y % 8));
    }
}

/**
 * @brief Внутренняя функция поиска глифа символа в шрифте.
 * @param[in] descr Указатель на дескриптор шрифта.
 * @param[in] ch Символ для поиска.
 * @return Указатель на глиф или NULL, если не найден.
 */
static const glyphs_t* find_glyph(const font_descriptor_t *descr, char ch)
{
	for (int i = 0; i < descr->glyphs_num; i++)
		if (descr->glyphs[i].ascii_code == ch)
			return &descr->glyphs[i];
	return NULL;
}

/**
 * @brief Внутренняя функция получения значения бита из массива битов.
 * @param[in] buff Указатель на массив битов.
 * @param[in] bit Номер бита.
 * @return Значение бита (0 или 1).
 */
static uint8_t get_bit_val(const uint8_t *buff, uint16_t bit)
{
	uint16_t byte = bit >> 3; // Номер байта
	bit &= 7;                 // Номер бита в байте
	return (buff[byte] & (1 << bit)) ? 1 : 0;
}

/**
 * @brief Вывод одного символа.
 * @param[in] ch Символ для вывода.
 * @param[in] font_idx Индекс шрифта из массива font_descr.
 * @param[in] isInvert Флаг инверсии цвета (1 - инвертировать, 0 - нет).
 * @return Код символа при успехе, -1 при ошибке.
 */
int ssd1306_WriteChar(char ch, uint8_t font_idx, uint8_t isInvert)
{
    // Проверка индекса шрифта
    if ( (sizeof(font_descr)/sizeof(font_descr[0])) <= font_idx ) return -1;
    const font_descriptor_t * descr = font_descr[font_idx];

    // Найти глиф символа
    const glyphs_t *glyph = find_glyph(descr, ch);
    uint8_t height = descr->font_height;
    // Если глиф не найден, использовать первый (обычно пробел)
    uint8_t width = glyph ? glyph->width : descr->glyphs[0].width;

    // Проверка оставшегося места на строке и высоте экрана
    if (SSD1306_WIDTH < (SSD1306.CurrentX + width) )
    	width = SSD1306_WIDTH - SSD1306.CurrentX;
    if (SSD1306_HEIGHT < (SSD1306.CurrentY + height) )
    	height = SSD1306_HEIGHT - SSD1306.CurrentY;
    
    // Использовать шрифт для рисования
    for(int y = 0; y < height; y++)
        for(int x = 0; x < width; x++)
        	if (!glyph)
                // Если глиф не найден, нарисовать пустой пиксель
                ssd1306_DrawPixel(SSD1306.CurrentX + x, SSD1306.CurrentY + y, SSD1306_COLOR_BLACK);
            else
            {
            	// Получить значение бита из данных шрифта
            	uint8_t c = get_bit_val(descr->font_data, glyph->bit_offset + y*width + x);
            	// Применить инверсию
            	c = isInvert ? !c : c;
                // Нарисовать пиксель с учётом цвета
                ssd1306_DrawPixel(SSD1306.CurrentX + x, SSD1306.CurrentY + y, c ? SSD1306_COLOR_WHITE : SSD1306_COLOR_BLACK);
            }

    // Сдвинуть курсор на ширину символа
    SSD1306.CurrentX += width;
    
    // Вернуть код символа для проверки
    return ch;
}

/**
 * @brief Вывод строки символов.
 * @param[in] str Указатель на строку (null-terminated).
 * @param[in] font_idx Индекс шрифта.
 * @param[in] isInvert Флаг инверсии цвета.
 * @return 0 при успехе, код проблемного символа при ошибке.
 */
char ssd1306_WriteString(char* str, uint8_t font_idx, uint8_t isInvert) {
    while (*str) {
        if (ssd1306_WriteChar(*str, font_idx, isInvert) != *str) {
            // Символ не удалось записать
            return *str;
        }
        str++;
    }
    
    // Всё прошло успешно
    return 0; // *str в этот момент равен 0
}

/**
 * @brief Установка курсора для вывода текста.
 * @param[in] x X координата курсора.
 * @param[in] y Y координата курсора.
 */
void ssd1306_SetCursor(uint8_t x, uint8_t y) {
    SSD1306.CurrentX = x;
    SSD1306.CurrentY = y;
}

/**
 * @brief Рисование линии алгоритмом Брезенхема.
 * @param[in] x1 X начала линии.
 * @param[in] y1 Y начала линии.
 * @param[in] x2 X конца линии.
 * @param[in] y2 Y конца линии.
 * @param[in] color Цвет линии.
 */
void ssd1306_Line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, SSD1306_COLOR color) {
    int32_t deltaX = abs(x2 - x1);
    int32_t deltaY = abs(y2 - y1);
    int32_t signX = ((x1 < x2) ? 1 : -1);
    int32_t signY = ((y1 < y2) ? 1 : -1);
    int32_t error = deltaX - deltaY;
    int32_t error2;
    
    // Нарисовать конечную точку
    ssd1306_DrawPixel(x2, y2, color);

    // Основной цикл алгоритма
    while((x1 != x2) || (y1 != y2)) {
        ssd1306_DrawPixel(x1, y1, color);
        error2 = error * 2;
        if(error2 > -deltaY) {
            error -= deltaY;
            x1 += signX;
        }
        
        if(error2 < deltaX) {
            error += deltaX;
            y1 += signY;
        }
    }
    return;
}

/**
 * @brief Преобразование градусов в радианы.
 * @param[in] par_deg Угол в градусах.
 * @return Угол в радианах.
 */
static float ssd1306_DegToRad(float par_deg) {
    return par_deg * (3.14f / 180.0f);
}

/**
 * @brief Нормализация угла к диапазону [0;360].
 * @param[in] par_deg Угол.
 * @return Нормализованный угол.
 */
static uint16_t ssd1306_NormalizeTo0_360(uint16_t par_deg) {
    uint16_t loc_angle;
    if(par_deg <= 360) {
        loc_angle = par_deg;
    } else {
        loc_angle = par_deg % 360;
        loc_angle = (loc_angle ? loc_angle : 360); // Если остаток 0, то 360
    }
    return loc_angle;
}

/**
 * @brief Рисование дуги окружности.
 *
 * Угол начинается от 4-й четверти тригонометрического круга (3pi/2).
 * @param[in] x Центр X дуги.
 * @param[in] y Центр Y дуги.
 * @param[in] radius Радиус дуги.
 * @param[in] start_angle Начальный угол в градусах.
 * @param[in] sweep Угол разворота в градусах.
 * @param[in] color Цвет дуги.
 */
void ssd1306_DrawArc(uint8_t x, uint8_t y, uint8_t radius, uint16_t start_angle, uint16_t sweep, SSD1306_COLOR color) {
    static const uint8_t CIRCLE_APPROXIMATION_SEGMENTS = 36;
    float approx_degree;
    uint32_t approx_segments;
    uint8_t xp1,xp2;
    uint8_t yp1,yp2;
    uint32_t count;
    uint32_t loc_sweep;
    float rad;
    
    loc_sweep = ssd1306_NormalizeTo0_360(sweep);
    
    count = (ssd1306_NormalizeTo0_360(start_angle) * CIRCLE_APPROXIMATION_SEGMENTS) / 360;
    approx_segments = (loc_sweep * CIRCLE_APPROXIMATION_SEGMENTS) / 360;
    approx_degree = loc_sweep / (float)approx_segments;
    while(count < approx_segments)
    {
        rad = ssd1306_DegToRad(count*approx_degree);
        xp1 = x + (int8_t)(sinf(rad)*radius);
        yp1 = y + (int8_t)(cosf(rad)*radius);    
        count++;
        if(count != approx_segments) {
            rad = ssd1306_DegToRad(count*approx_degree);
        } else {
            rad = ssd1306_DegToRad(loc_sweep);
        }
        xp2 = x + (int8_t)(sinf(rad)*radius);
        yp2 = y + (int8_t)(cosf(rad)*radius);    
        ssd1306_Line(xp1,yp1,xp2,yp2,color);
    }
    
    return;
}

/**
 * @brief Рисование дуги окружности с линиями радиуса.
 *
 * Угол начинается от 4-й четверти тригонометрического круга (3pi/2).
 * @param[in] x Центр X дуги.
 * @param[in] y Центр Y дуги.
 * @param[in] radius Радиус дуги.
 * @param[in] start_angle Начальный угол в градусах.
 * @param[in] sweep Угол разворота в градусах.
 * @param[in] color Цвет дуги и линий.
 */
void ssd1306_DrawArcWithRadiusLine(uint8_t x, uint8_t y, uint8_t radius, uint16_t start_angle, uint16_t sweep, SSD1306_COLOR color) {
    const uint32_t CIRCLE_APPROXIMATION_SEGMENTS = 36;
    float approx_degree;
    uint32_t approx_segments;
    uint8_t xp1;
    uint8_t xp2 = 0;
    uint8_t yp1;
    uint8_t yp2 = 0;
    uint32_t count;
    uint32_t loc_sweep;
    float rad;
    
    loc_sweep = ssd1306_NormalizeTo0_360(sweep);
    
    count = (ssd1306_NormalizeTo0_360(start_angle) * CIRCLE_APPROXIMATION_SEGMENTS) / 360;
    approx_segments = (loc_sweep * CIRCLE_APPROXIMATION_SEGMENTS) / 360;
    approx_degree = loc_sweep / (float)approx_segments;

    rad = ssd1306_DegToRad(count*approx_degree);
    uint8_t first_point_x = x + (int8_t)(sinf(rad)*radius);
    uint8_t first_point_y = y + (int8_t)(cosf(rad)*radius);   
    while (count < approx_segments) {
        rad = ssd1306_DegToRad(count*approx_degree);
        xp1 = x + (int8_t)(sinf(rad)*radius);
        yp1 = y + (int8_t)(cosf(rad)*radius);    
        count++;
        if (count != approx_segments) {
            rad = ssd1306_DegToRad(count*approx_degree);
        } else {
            rad = ssd1306_DegToRad(loc_sweep);
        }
        xp2 = x + (int8_t)(sinf(rad)*radius);
        yp2 = y + (int8_t)(cosf(rad)*radius);    
        ssd1306_Line(xp1,yp1,xp2,yp2,color);
    }
    
    // Линии радиуса
    ssd1306_Line(x,y,first_point_x,first_point_y,color);
    ssd1306_Line(x,y,xp2,yp2,color);
    return;
}

/**
 * @brief Рисование окружности алгоритмом Брезенхема.
 * @param[in] par_x Центр X окружности.
 * @param[in] par_y Центр Y окружности.
 * @param[in] par_r Радиус окружности.
 * @param[in] par_color Цвет окружности.
 */
void ssd1306_DrawCircle(uint8_t par_x,uint8_t par_y,uint8_t par_r,SSD1306_COLOR par_color) {
    int32_t x = -par_r;
    int32_t y = 0;
    int32_t err = 2 - 2 * par_r;
    int32_t e2;

    // Проверка границ
    if (par_x >= SSD1306_WIDTH || par_y >= SSD1306_HEIGHT) {
        return;
    }

    do {
        // Рисование 4 пикселей, симметричных относительно осей
        ssd1306_DrawPixel(par_x - x, par_y + y, par_color);
        ssd1306_DrawPixel(par_x + x, par_y + y, par_color);
        ssd1306_DrawPixel(par_x + x, par_y - y, par_color);
        ssd1306_DrawPixel(par_x - x, par_y - y, par_color);
        e2 = err;

        if (e2 <= y) {
            y++;
            err = err + (y * 2 + 1);
            if(-x == y && e2 <= x) {
                e2 = 0;
            }
        }

        if (e2 > x) {
            x++;
            err = err + (x * 2 + 1);
        }
    } while (x <= 0);

    return;
}

/**
 * @brief Рисование закрашенного круга.
 *
 * Пиксельные позиции рассчитываются с помощью алгоритма Брезенхема.
 * @param[in] par_x Центр X круга.
 * @param[in] par_y Центр Y круга.
 * @param[in] par_r Радиус круга.
 * @param[in] par_color Цвет круга.
 */
void ssd1306_FillCircle(uint8_t par_x,uint8_t par_y,uint8_t par_r,SSD1306_COLOR par_color) {
    int32_t x = -par_r;
    int32_t y = 0;
    int32_t err = 2 - 2 * par_r;
    int32_t e2;

    // Проверка границ
    if (par_x >= SSD1306_WIDTH || par_y >= SSD1306_HEIGHT) {
        return;
    }

    do {
        // Заполнение горизонтальной линии между симметричными точками
        for (uint8_t _y = (par_y + y); _y >= (par_y - y); _y--) {
            for (uint8_t _x = (par_x - x); _x >= (par_x + x); _x--) {
                ssd1306_DrawPixel(_x, _y, par_color);
            }
        }

        e2 = err;
        if (e2 <= y) {
            y++;
            err = err + (y * 2 + 1);
            if (-x == y && e2 <= x) {
                e2 = 0;
            }
        }

        if (e2 > x) {
            x++;
            err = err + (x * 2 + 1);
        }
    } while (x <= 0);

    return;
}

/**
 * @brief Рисование прямоугольника (только границы).
 * @param[in] x1 X верхнего левого угла.
 * @param[in] y1 Y верхнего левого угла.
 * @param[in] x2 X нижнего правого угла.
 * @param[in] y2 Y нижнего правого угла.
 * @param[in] color Цвет прямоугольника.
 */
void ssd1306_DrawRectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, SSD1306_COLOR color) {
    ssd1306_Line(x1,y1,x2,y1,color); // Верх
    ssd1306_Line(x2,y1,x2,y2,color); // Право
    ssd1306_Line(x2,y2,x1,y2,color); // Низ
    ssd1306_Line(x1,y2,x1,y1,color); // Лево

    return;
}

/**
 * @brief Рисование закрашенного прямоугольника.
 * @param[in] x1 X верхнего левого угла.
 * @param[in] y1 Y верхнего левого угла.
 * @param[in] x2 X нижнего правого угла.
 * @param[in] y2 Y нижнего правого угла.
 * @param[in] color Цвет прямоугольника.
 */
void ssd1306_FillRectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, SSD1306_COLOR color) {
    // Найти минимальные и максимальные координаты
    uint8_t x_start = ((x1<=x2) ? x1 : x2);
    uint8_t x_end   = ((x1<=x2) ? x2 : x1);
    uint8_t y_start = ((y1<=y2) ? y1 : y2);
    uint8_t y_end   = ((y1<=y2) ? y2 : y1);

    // Пройти по всем пикселям внутри прямоугольника
    for (uint8_t y= y_start; (y<= y_end)&&(y<SSD1306_HEIGHT); y++) {
        for (uint8_t x= x_start; (x<= x_end)&&(x<SSD1306_WIDTH); x++) {
            ssd1306_DrawPixel(x, y, color);
        }
    }
    return;
}

/**
 * @brief Инвертировать цвет пикселей в прямоугольнике (включая границы).
 * @param[in] x1 X верхнего левого угла.
 * @param[in] y1 Y верхнего левого угла.
 * @param[in] x2 X нижнего правого угла.
 * @param[in] y2 Y нижнего правого угла.
 * @return 1 при успехе, -1 при ошибке (выход за границы или неверный порядок координат).
 */
int ssd1306_InvertRectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
  // Проверка границ
  if ((x2 >= SSD1306_WIDTH) || (y2 >= SSD1306_HEIGHT)) {
    return -1;
  }
  // Проверка порядка координат
  if ((x1 > x2) || (y1 > y2)) {
    return -1;
  }
  uint32_t i;
  // Если прямоугольник НЕ лежит на одной 8-пиксельной строке (разные страницы Y)
  if ((y1 / 8) != (y2 / 8)) {
    /* если прямоугольник не лежит на одной строке 8px */
    for (uint32_t x = x1; x <= x2; x++) {
      // Инвертировать биты в первом байте (верхняя строка байта)
      i = x + (y1 / 8) * SSD1306_WIDTH;
      SSD1306_Buffer[i] ^= 0xFF << (y1 % 8);
      // Инвертировать целые байты посередине (если есть)
      i += SSD1306_WIDTH;
      for (; i < x + (y2 / 8) * SSD1306_WIDTH; i += SSD1306_WIDTH) {
        SSD1306_Buffer[i] ^= 0xFF;
      }
      // Инвертировать биты в последнем байте (нижняя строка байта)
      SSD1306_Buffer[i] ^= 0xFF >> (7 - (y2 % 8));
    }
  } else {
    /* если прямоугольник лежит на одной строке 8px */
    // Маска для битов в пределах одной страницы Y
    const uint8_t mask = (0xFF << (y1 % 8)) & (0xFF >> (7 - (y2 % 8)));
    // Инвертировать биты в байтах, покрываемых прямоугольником
    for (i = x1 + (y1 / 8) * SSD1306_WIDTH;
         i <= (uint32_t)x2 + (y2 / 8) * SSD1306_WIDTH; i++) {
      SSD1306_Buffer[i] ^= mask;
    }
  }
  return 1;
}

/**
 * @brief Рисование битмапа (монохромного изображения).
 * @param[in] x X координата верхнего левого угла битмапа.
 * @param[in] y Y координата верхнего левого угла битмапа.
 * @param[in] bitmap Указатель на массив данных битмапа.
 * @param[in] w Ширина битмапа в пикселях.
 * @param[in] h Высота битмапа в пикселях.
 * @param[in] color Цвет пикселей битмапа.
 */
void ssd1306_DrawBitmap(uint8_t x, uint8_t y, const unsigned char* bitmap, uint8_t w, uint8_t h, SSD1306_COLOR color) {
    // Ширина строки битмапа в байтах (с выравниванием до целого байта)
    int16_t byteWidth = (w + 7) / 8;
    uint8_t byte = 0;

    // Проверка границ
    if (x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT) {
        return;
    }

    // Пройти по строкам битмапа
    for (uint8_t j = 0; j < h; j++, y++) {
        // Пройти по пикселям в строке
        for (uint8_t i = 0; i < w; i++) {
            // Сдвигаем байт, если не первый бит в байте
            if (i & 7) {
                byte <<= 1;
            } else {
                // Загружаем новый байт из битмапа
                byte = (*(const unsigned char *)(&bitmap[j * byteWidth + i / 8]));
            }

            // Если бит установлен, рисуем пиксель
            if (byte & 0x80) {
                ssd1306_DrawPixel(x + i, y, color);
            }
        }
    }
    return;
}

/**
 * @brief Установка контрастности дисплея.
 * @param[in] value Значение контрастности (обычно 0-255).
 */
void ssd1306_SetContrast(const uint8_t value) {
    const uint8_t kSetContrastControlRegister = 0x81;
    ssd1306_WriteCommand(kSetContrastControlRegister);
    ssd1306_WriteCommand(value);
}

/**
 * @brief Включение/выключение дисплея.
 * @param[in] on 0 - выключить, любое другое значение - включить.
 */
void ssd1306_SetDisplayOn(const uint8_t on) {
    uint8_t value;
    if (on) {
        value = 0xAF;   // Включить дисплей
        SSD1306.DisplayOn = 1;
    } else {
        value = 0xAE;   // Выключить дисплей
        SSD1306.DisplayOn = 0;
    }
    ssd1306_WriteCommand(value);
}

/**
 * @brief Чтение состояния включения дисплея.
 * @return 0, если дисплей выключен.
 *         1, если дисплей включён.
 */
uint8_t ssd1306_GetDisplayOn() {
    return SSD1306.DisplayOn;
}