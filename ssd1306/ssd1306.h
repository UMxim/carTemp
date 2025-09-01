/**
 * This Library was originally written by Olivier Van den Eede (4ilo) in 2016.
 * Some refactoring was done and SPI support was added by Aleksander Alekseev (afiskon) in 2018.
 *
 * https://github.com/afiskon/stm32-ssd1306
 */

#ifndef __SSD1306_H__
#define __SSD1306_H__

#include <stddef.h>
#include <stdint.h>
//#include <_ansi.h>


#define SSD1306_I2C_ADDR        (0x3C << 1)
#define SSD1306_HEIGHT          32
#define SSD1306_WIDTH           128
#define SSD1306_BUFFER_SIZE   (SSD1306_WIDTH * SSD1306_HEIGHT / 8)

//_BEGIN_STD_C

//#include "ssd1306_conf.h"

#ifdef SSD1306_X_OFFSET
#define SSD1306_X_OFFSET_LOWER (SSD1306_X_OFFSET & 0x0F)
#define SSD1306_X_OFFSET_UPPER ((SSD1306_X_OFFSET >> 4) & 0x07)
#else
#define SSD1306_X_OFFSET_LOWER 0
#define SSD1306_X_OFFSET_UPPER 0
#endif

// Struct
typedef struct {
    uint16_t CurrentX;
    uint16_t CurrentY;
    uint8_t Initialized;
    uint8_t DisplayOn;
} SSD1306_t;

typedef struct {
    uint8_t x;
    uint8_t y;
} SSD1306_VERTEX;

/** Font */
typedef struct {
	const uint8_t width;                /**< Font width in pixels */
	const uint8_t height;               /**< Font height in pixels */
	const uint16_t *const data;         /**< Pointer to font data array */
    const uint8_t *const char_width;    /**< Proportional character width in pixels (NULL for monospaced) */
} SSD1306_Font_t;

// Procedure definitions
void ssd1306_Init(int(*rxCallback)(uint8_t*buff, uint16_t size), void(*delayCallback)(uint32_t deelay_ms));
void ssd1306_Fill(uint8_t color);
void ssd1306_UpdateScreen(void);
void ssd1306_DrawPixel(uint8_t x, uint8_t y, uint8_t color);
char ssd1306_WriteChar(char ch, uint8_t font_h, uint8_t color);
char ssd1306_WriteString(char* str, uint8_t font_h, uint8_t color);
void ssd1306_SetCursor(uint8_t x, uint8_t y);
void ssd1306_Line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color);
void ssd1306_DrawArc(uint8_t x, uint8_t y, uint8_t radius, uint16_t start_angle, uint16_t sweep, uint8_t color);
void ssd1306_DrawArcWithRadiusLine(uint8_t x, uint8_t y, uint8_t radius, uint16_t start_angle, uint16_t sweep, uint8_t color);
void ssd1306_DrawCircle(uint8_t par_x, uint8_t par_y, uint8_t par_r, uint8_t color);
void ssd1306_FillCircle(uint8_t par_x,uint8_t par_y,uint8_t par_r,uint8_t par_color);
void ssd1306_Polyline(const SSD1306_VERTEX *par_vertex, uint16_t par_size, uint8_t color);
void ssd1306_DrawRectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color);
void ssd1306_FillRectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color);

/**
 * @brief Invert color of pixels in rectangle (include border)
 * 
 * @param x1 X Coordinate of top left corner
 * @param y1 Y Coordinate of top left corner
 * @param x2 X Coordinate of bottom right corner
 * @param y2 Y Coordinate of bottom right corner
 * @return SSD1306_Error_t status
 */
int ssd1306_InvertRectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);

void ssd1306_DrawBitmap(uint8_t x, uint8_t y, const unsigned char* bitmap, uint8_t w, uint8_t h, uint8_t color);

/**
 * @brief Sets the contrast of the display.
 * @param[in] value contrast to set.
 * @note Contrast increases as the value increases.
 * @note RESET = 7Fh.
 */
void ssd1306_SetContrast(const uint8_t value);

/**
 * @brief Set Display ON/OFF.
 * @param[in] on 0 for OFF, any for ON.
 */
void ssd1306_SetDisplayOn(const uint8_t on);

/**
 * @brief Reads DisplayOn state.
 * @return  0: OFF.
 *          1: ON.
 */
uint8_t ssd1306_GetDisplayOn(void);

// Low-level procedures
void ssd1306_Reset(void);
void ssd1306_WriteCommand(uint8_t byte);
void ssd1306_WriteData(uint8_t* buffer, size_t buff_size);
int ssd1306_FillBuffer(uint8_t* buf, uint32_t len);

void myChar(char ch, const uint8_t * buff, uint8_t dest_w, uint8_t dest_h);
//_END_STD_C

#endif // __SSD1306_H__
