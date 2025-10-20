/**
 * @file ssd1306.h
 * @brief Заголовочный файл библиотеки для работы с OLED-дисплеями SSD1306.
 *
 * Библиотека предоставляет функции для инициализации, очистки, обновления
 * экрана, рисования пикселей, линий, фигур, вывода текста и битмапов
 * на OLED-дисплей с контроллером SSD1306 (или совместимым, например SH1106).
 * Поддерживается интерфейс I2C и 4-проводной SPI.
 *
 * Основано на коде Olivier Van den Eede (4ilo) 2016 г.
 * Дальнейшая разработка и добавление SPI: Aleksander Alekseev (afiskon) 2018 г.
 * https://github.com/afiskon/stm32-ssd1306
 *
 * Логика работы:
 * - Инициализация: ssd1306_Init настраивает дисплей и внутренний буфер.
 * - Рисование: Функции (DrawPixel, Line, Circle и т.д.) модифицируют
 *   внутренний буфер изображения.
 * - Обновление: ssd1306_UpdateScreen отправляет содержимое буфера на дисплей.
 * - Низкий уровень: ssd1306_WriteCommand и ssd1306_WriteData используются
 *   для отправки команд и данных дисплею через I2C/SPI.
 */

#ifndef __SSD1306_H__
#define __SSD1306_H__

#include <stddef.h>
#include <stdint.h>
//#include <_ansi.h> // Комментарий: Этот заголовок может быть не нужен.

// --- Конфигурация дисплея ---
/// @brief I2C адрес дисплея (7-битный, сдвинутый влево).
#define SSD1306_I2C_ADDR        (0x3C << 1)
/// @brief Высота дисплея в пикселях.
#define SSD1306_HEIGHT          32
/// @brief Ширина дисплея в пикселях.
#define SSD1306_WIDTH           128
/// @brief Размер внутреннего буфера изображения (в байтах).
#define SSD1306_BUFFER_SIZE   (SSD1306_WIDTH * SSD1306_HEIGHT / 8)

// --- Настройки отображения ---
/// @brief Опциональное вертикальное отражение изображения.
#define SSD1306_MIRROR_VERT
/// @brief Опциональное горизонтальное отражение изображения.
#define SSD1306_MIRROR_HORIZ
//_BEGIN_STD_C // Комментарий: Этот макрос, вероятно, не используется.

//#include "ssd1306_conf.h" // Комментарий: Файл конфигурации может быть подключен здесь.

// --- Смещение по X (если поддерживается драйвером) ---
#ifdef SSD1306_X_OFFSET
#define SSD1306_X_OFFSET_LOWER (SSD1306_X_OFFSET & 0x0F) ///< Младшие 4 бита смещения X.
#define SSD1306_X_OFFSET_UPPER ((SSD1306_X_OFFSET >> 4) & 0x07) ///< Старшие 3 бита смещения X.
#else
#define SSD1306_X_OFFSET_LOWER 0 ///< Смещение X по умолчанию.
#define SSD1306_X_OFFSET_UPPER 0 ///< Смещение X по умолчанию.
#endif

// --- Типы данных ---

/**
 * @brief Структура для представления точки (вершины).
 */
typedef struct {
    uint8_t x; ///< Координата X.
    uint8_t y; ///< Координата Y.
} SSD1306_VERTEX_t;

/**
 * @brief Перечисление цветов пикселей.
 */
typedef enum {
	SSD1306_COLOR_WHITE = 1,  ///< Белый (включённый пиксель).
	SSD1306_COLOR_BLACK = !SSD1306_COLOR_WHITE ///< Чёрный (выключенный пиксель).
} SSD1306_COLOR;

// --- Объявления процедур (API) ---

/**
 * @brief Инициализация дисплея SSD1306.
 * Настраивает дисплей и внутренний буфер.
 */
void ssd1306_Init();

/**
 * @brief Заливка всего экрана указанным цветом.
 * @param[in] color Цвет для заливки (SSD1306_COLOR_WHITE или SSD1306_COLOR_BLACK).
 */
void ssd1306_Fill(SSD1306_COLOR color);

/**
 * @brief Обновление экрана дисплея.
 * Отправляет содержимое внутреннего буфера на физический дисплей.
 */
void ssd1306_UpdateScreen(void);

/**
 * @brief Рисование одного пикселя.
 * @param[in] x Координата X пикселя (0 - SSD1306_WIDTH-1).
 * @param[in] y Координата Y пикселя (0 - SSD1306_HEIGHT-1).
 * @param[in] color Цвет пикселя.
 */
void ssd1306_DrawPixel(uint8_t x, uint8_t y, SSD1306_COLOR color);

/**
 * @brief Вывод одного символа.
 * @param[in] ch Символ для вывода.
 * @param[in] font_idx Индекс шрифта.
 * @param[in] isInvert Флаг инверсии цвета (1 - инвертировать, 0 - нет).
 * @return Код результата (0 - успех, <0 - ошибка).
 */
int ssd1306_WriteChar(char ch, uint8_t font_idx, uint8_t isInvert);

/**
 * @brief Вывод строки символов.
 * @param[in] str Указатель на строку (null-terminated).
 * @param[in] font_h Индекс шрифта.
 * @param[in] isInvert Флаг инверсии цвета.
 * @return Количество успешно выведенных символов.
 */
char ssd1306_WriteString(char* str, uint8_t font_h, uint8_t isInvert);

/**
 * @brief Установка курсора для вывода текста.
 * @param[in] x Координата X курсора.
 * @param[in] y Координата Y курсора.
 */
void ssd1306_SetCursor(uint8_t x, uint8_t y);

/**
 * @brief Рисование линии.
 * @param[in] x1 Координата X начала.
 * @param[in] y1 Координата Y начала.
 * @param[in] x2 Координата X конца.
 * @param[in] y2 Координата Y конца.
 * @param[in] color Цвет линии.
 */
void ssd1306_Line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, SSD1306_COLOR color);

/**
 * @brief Рисование дуги окружности.
 * @param[in] x Центр X дуги.
 * @param[in] y Центр Y дуги.
 * @param[in] radius Радиус дуги.
 * @param[in] start_angle Начальный угол дуги (в градусах).
 * @param[in] sweep Угол разворота дуги (в градусах).
 * @param[in] color Цвет дуги.
 */
void ssd1306_DrawArc(uint8_t x, uint8_t y, uint8_t radius, uint16_t start_angle, uint16_t sweep, SSD1306_COLOR color);

/**
 * @brief Рисование дуги окружности с линиями радиуса.
 * @param[in] x Центр X дуги.
 * @param[in] y Центр Y дуги.
 * @param[in] radius Радиус дуги.
 * @param[in] start_angle Начальный угол дуги.
 * @param[in] sweep Угол разворота дуги.
 * @param[in] color Цвет дуги и линий.
 */
void ssd1306_DrawArcWithRadiusLine(uint8_t x, uint8_t y, uint8_t radius, uint16_t start_angle, uint16_t sweep, SSD1306_COLOR color);

/**
 * @brief Рисование окружности.
 * @param[in] par_x Центр X окружности.
 * @param[in] par_y Центр Y окружности.
 * @param[in] par_r Радиус окружности.
 * @param[in] color Цвет окружности.
 */
void ssd1306_DrawCircle(uint8_t par_x, uint8_t par_y, uint8_t par_r, SSD1306_COLOR color);

/**
 * @brief Рисование закрашенного круга.
 * @param[in] par_x Центр X круга.
 * @param[in] par_y Центр Y круга.
 * @param[in] par_r Радиус круга.
 * @param[in] par_color Цвет круга.
 */
void ssd1306_FillCircle(uint8_t par_x,uint8_t par_y,uint8_t par_r,SSD1306_COLOR par_color);

/**
 * @brief Рисование прямоугольника (только границы).
 * @param[in] x1 Координата X верхнего левого угла.
 * @param[in] y1 Координата Y верхнего левого угла.
 * @param[in] x2 Координата X нижнего правого угла.
 * @param[in] y2 Координата Y нижнего правого угла.
 * @param[in] color Цвет прямоугольника.
 */
void ssd1306_DrawRectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, SSD1306_COLOR color);

/**
 * @brief Рисование закрашенного прямоугольника.
 * @param[in] x1 Координата X верхнего левого угла.
 * @param[in] y1 Координата Y верхнего левого угла.
 * @param[in] x2 Координата X нижнего правого угла.
 * @param[in] y2 Координата Y нижнего правого угла.
 * @param[in] color Цвет прямоугольника.
 */
void ssd1306_FillRectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, SSD1306_COLOR color);

/**
 * @brief Инвертировать цвет пикселей в прямоугольнике (включая границы).
 * @param[in] x1 X Координата верхнего левого угла.
 * @param[in] y1 Y Координата верхнего левого угла.
 * @param[in] x2 X Координата нижнего правого угла.
 * @param[in] y2 Y Координата нижнего правого угла.
 * @return Код результата (0 - успех, <0 - ошибка).
 */
int ssd1306_InvertRectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);

/**
 * @brief Рисование битмапа (монохромного изображения).
 * @param[in] x Координата X верхнего левого угла битмапа.
 * @param[in] y Координата Y верхнего левого угла битмапа.
 * @param[in] bitmap Указатель на массив данных битмапа.
 * @param[in] w Ширина битмапа в пикселях.
 * @param[in] h Высота битмапа в пикселях.
 * @param[in] color Цвет пикселей битмапа.
 */
void ssd1306_DrawBitmap(uint8_t x, uint8_t y, const unsigned char* bitmap, uint8_t w, uint8_t h, uint8_t color);

/**
 * @brief Установка контрастности дисплея.
 * @param[in] value Значение контрастности (обычно 0-255).
 * @note Контрастность увеличивается с ростом значения.
 * @note Значение по умолчанию после сброса: 0x7F.
 */
void ssd1306_SetContrast(const uint8_t value);

/**
 * @brief Включение/выключение дисплея.
 * @param[in] on 0 - выключить, любое другое значение - включить.
 */
void ssd1306_SetDisplayOn(const uint8_t on);

/**
 * @brief Чтение состояния включения дисплея.
 * @return 0, если дисплей выключен.
 *         1, если дисплей включён.
 */
uint8_t ssd1306_GetDisplayOn(void);

// --- Низкоуровневые процедуры (для внутреннего использования или расширения) ---

/**
 * @brief Отправка команды дисплею.
 * @param[in] byte Байт команды.
 */
void ssd1306_WriteCommand(uint8_t byte);

/**
 * @brief Отправка данных дисплею.
 * @param[in] reg Регистр данных (обычно 0x40 для SSD1306).
 * @param[in] buffer Указатель на буфер с данными.
 * @param[in] buff_size Размер буфера данных.
 */
void ssd1306_WriteData(uint8_t reg, uint8_t* buffer, size_t buff_size);

/**
 * @brief Заполнение внутреннего буфера изображения данными.
 * @param[in] buf Указатель на буфер с данными.
 * @param[in] len Количество байт для копирования.
 * @return Код результата (0 - успех, <0 - ошибка).
 */
int ssd1306_FillBuffer(uint8_t* buf, uint32_t len);

// --- Вспомогательные функции (возможно, для внутреннего использования) ---

/**
 * @brief Внутренняя функция для отрисовки символа из буфера.
 * @param[in] ch Символ.
 * @param[in] buff Указатель на буфер данных символа.
 * @param[in] dest_w Ширина символа.
 * @param[in] dest_h Высота символа.
 */
void myChar(char ch, const uint8_t * buff, uint8_t dest_w, uint8_t dest_h);

//_END_STD_C // Комментарий: Этот макрос, вероятно, не используется.

#endif // __SSD1306_H__