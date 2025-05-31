#include "ssd1306.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>  // For memcpy

#if defined(SSD1306_USE_I2C)

void ssd1306_Reset(void) {
    /* for I2C - do nothing */
}

// Send a byte to the command register
void ssd1306_WriteCommand(uint8_t byte) {
    HAL_I2C_Mem_Write(&SSD1306_I2C_PORT, SSD1306_I2C_ADDR, 0x00, 1, &byte, 1, HAL_MAX_DELAY);
}

// Send data
void ssd1306_WriteData(uint8_t* buffer, size_t buff_size) {
    HAL_I2C_Mem_Write(&SSD1306_I2C_PORT, SSD1306_I2C_ADDR, 0x40, 1, buffer, buff_size, HAL_MAX_DELAY);
}

#elif defined(SSD1306_USE_SPI)

void ssd1306_Reset(void) {
    // CS = High (not selected)
    HAL_GPIO_WritePin(SSD1306_CS_Port, SSD1306_CS_Pin, GPIO_PIN_SET);

    // Reset the OLED
    HAL_GPIO_WritePin(SSD1306_Reset_Port, SSD1306_Reset_Pin, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(SSD1306_Reset_Port, SSD1306_Reset_Pin, GPIO_PIN_SET);
    HAL_Delay(10);
}

// Send a byte to the command register
void ssd1306_WriteCommand(uint8_t byte) {
    HAL_GPIO_WritePin(SSD1306_CS_Port, SSD1306_CS_Pin, GPIO_PIN_RESET); // select OLED
    HAL_GPIO_WritePin(SSD1306_DC_Port, SSD1306_DC_Pin, GPIO_PIN_RESET); // command
    HAL_SPI_Transmit(&SSD1306_SPI_PORT, (uint8_t *) &byte, 1, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(SSD1306_CS_Port, SSD1306_CS_Pin, GPIO_PIN_SET); // un-select OLED
}

// Send data
void ssd1306_WriteData(uint8_t* buffer, size_t buff_size) {
    HAL_GPIO_WritePin(SSD1306_CS_Port, SSD1306_CS_Pin, GPIO_PIN_RESET); // select OLED
    HAL_GPIO_WritePin(SSD1306_DC_Port, SSD1306_DC_Pin, GPIO_PIN_SET); // data
    HAL_SPI_Transmit(&SSD1306_SPI_PORT, buffer, buff_size, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(SSD1306_CS_Port, SSD1306_CS_Pin, GPIO_PIN_SET); // un-select OLED
}

#else
#error "You should define SSD1306_USE_SPI or SSD1306_USE_I2C macro"
#endif


// Screenbuffer
static uint8_t SSD1306_Buffer[SSD1306_BUFFER_SIZE];

// Screen object
static SSD1306_t SSD1306;

/* Fills the Screenbuffer with values from a given buffer of a fixed length */
SSD1306_Error_t ssd1306_FillBuffer(uint8_t* buf, uint32_t len) {
    SSD1306_Error_t ret = SSD1306_ERR;
    if (len <= SSD1306_BUFFER_SIZE) {
        memcpy(SSD1306_Buffer,buf,len);
        ret = SSD1306_OK;
    }
    return ret;
}

/* Initialize the oled screen */
void ssd1306_Init(void) {
    // Reset OLED
    ssd1306_Reset();

    // Wait for the screen to boot
    HAL_Delay(100);

    // Init OLED
    ssd1306_SetDisplayOn(0); //display off

    ssd1306_WriteCommand(0x20); //Set Memory Addressing Mode
    ssd1306_WriteCommand(0x00); // 00b,Horizontal Addressing Mode; 01b,Vertical Addressing Mode;
                                // 10b,Page Addressing Mode (RESET); 11b,Invalid

    ssd1306_WriteCommand(0xB0); //Set Page Start Address for Page Addressing Mode,0-7

#ifdef SSD1306_MIRROR_VERT
    ssd1306_WriteCommand(0xC0); // Mirror vertically
#else
    ssd1306_WriteCommand(0xC8); //Set COM Output Scan Direction
#endif

    ssd1306_WriteCommand(0x00); //---set low column address
    ssd1306_WriteCommand(0x10); //---set high column address

    ssd1306_WriteCommand(0x40); //--set start line address - CHECK

    ssd1306_SetContrast(0xFF);

#ifdef SSD1306_MIRROR_HORIZ
    ssd1306_WriteCommand(0xA0); // Mirror horizontally
#else
    ssd1306_WriteCommand(0xA1); //--set segment re-map 0 to 127 - CHECK
#endif

#ifdef SSD1306_INVERSE_COLOR
    ssd1306_WriteCommand(0xA7); //--set inverse color
#else
    ssd1306_WriteCommand(0xA6); //--set normal color
#endif

// Set multiplex ratio.
#if (SSD1306_HEIGHT == 128)
    // Found in the Luma Python lib for SH1106.
    ssd1306_WriteCommand(0xFF);
#else
    ssd1306_WriteCommand(0xA8); //--set multiplex ratio(1 to 64) - CHECK
#endif

#if (SSD1306_HEIGHT == 32)
    ssd1306_WriteCommand(0x1F); //
#elif (SSD1306_HEIGHT == 64)
    ssd1306_WriteCommand(0x3F); //
#elif (SSD1306_HEIGHT == 128)
    ssd1306_WriteCommand(0x3F); // Seems to work for 128px high displays too.
#else
#error "Only 32, 64, or 128 lines of height are supported!"
#endif

    ssd1306_WriteCommand(0xA4); //0xa4,Output follows RAM content;0xa5,Output ignores RAM content

    ssd1306_WriteCommand(0xD3); //-set display offset - CHECK
    ssd1306_WriteCommand(0x00); //-not offset

    ssd1306_WriteCommand(0xD5); //--set display clock divide ratio/oscillator frequency
    ssd1306_WriteCommand(0xF0); //--set divide ratio

    ssd1306_WriteCommand(0xD9); //--set pre-charge period
    ssd1306_WriteCommand(0x22); //

    ssd1306_WriteCommand(0xDA); //--set com pins hardware configuration - CHECK
#if (SSD1306_HEIGHT == 32)
    ssd1306_WriteCommand(0x02);
#elif (SSD1306_HEIGHT == 64)
    ssd1306_WriteCommand(0x12);
#elif (SSD1306_HEIGHT == 128)
    ssd1306_WriteCommand(0x12);
#else
#error "Only 32, 64, or 128 lines of height are supported!"
#endif

    ssd1306_WriteCommand(0xDB); //--set vcomh
    ssd1306_WriteCommand(0x20); //0x20,0.77xVcc

    ssd1306_WriteCommand(0x8D); //--set DC-DC enable
    ssd1306_WriteCommand(0x14); //
    ssd1306_SetDisplayOn(1); //--turn on SSD1306 panel

    // Clear screen
    ssd1306_Fill(Black);
    
    // Flush buffer to screen
    ssd1306_UpdateScreen();
    
    // Set default values for screen object
    SSD1306.CurrentX = 0;
    SSD1306.CurrentY = 0;
    
    SSD1306.Initialized = 1;
}

/* Fill the whole screen with the given color */
void ssd1306_Fill(SSD1306_COLOR color) {
    memset(SSD1306_Buffer, (color == Black) ? 0x00 : 0xFF, sizeof(SSD1306_Buffer));
}

/* Write the screenbuffer with changed to the screen */
void ssd1306_UpdateScreen(void) {
    // Write data to each page of RAM. Number of pages
    // depends on the screen height:
    //
    //  * 32px   ==  4 pages
    //  * 64px   ==  8 pages
    //  * 128px  ==  16 pages
    for(uint8_t i = 0; i < SSD1306_HEIGHT/8; i++) {
        ssd1306_WriteCommand(0xB0 + i); // Set the current RAM page address.
        ssd1306_WriteCommand(0x00 + SSD1306_X_OFFSET_LOWER);
        ssd1306_WriteCommand(0x10 + SSD1306_X_OFFSET_UPPER);
        ssd1306_WriteData(&SSD1306_Buffer[SSD1306_WIDTH*i],SSD1306_WIDTH);
    }
}

/*
 * Draw one pixel in the screenbuffer
 * X => X Coordinate
 * Y => Y Coordinate
 * color => Pixel color
 */
void ssd1306_DrawPixel(uint8_t x, uint8_t y, SSD1306_COLOR color) {
    if(x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT) {
        // Don't write outside the buffer
        return;
    }
   
    // Draw in the right color
    if(color == White) {
        SSD1306_Buffer[x + (y / 8) * SSD1306_WIDTH] |= 1 << (y % 8);
    } else { 
        SSD1306_Buffer[x + (y / 8) * SSD1306_WIDTH] &= ~(1 << (y % 8));
    }
}

/*
 * Draw 1 char to the screen buffer
 * ch       => char om weg te schrijven
 * Font     => Font waarmee we gaan schrijven
 * color    => Black or White
 */

const uint8_t big8[] =
{
	0x12, 0x20,//W, H
	0x00,0x00,0x00,0x00,0x00,0x00,0x3E,0x00,0x00,0x1F,0x7F,0x00,0x80,0xBF,0xFF,0x00,
	0x80,0xBF,0xFF,0x00,0xC0,0xBF,0xFF,0x01,0xC0,0xFF,0x80,0x01,0x40,0x40,0x00,0x01,
	0x40,0x40,0x00,0x01,0xC0,0x60,0x00,0x01,0xC0,0xFF,0x80,0x01,0xC0,0xFF,0xFF,0x01,
	0x80,0xBF,0xFF,0x00,0x80,0xBF,0xFF,0x00,0x00,0x1F,0x7F,0x00,0x00,0x00,0x3E	// 56
};





const uint8_t SmallFont[] =				//	Шрифт	SmallFont
{												//
0x06, 0x08, 0x20, 0x5F,							//	ширина символов (6), высота символов (8), код первого символа (32), количество символов (95)
	
	/////
	
	0x00, 0x36, 0x49, 0x49, 0x49, 0x36,				//	025)	0x38=056	8
	
	
	
												//
0x00, 0x00, 0x00, 0x00, 0x00, 0x00,				//	001)	0x20=032	пробел
0x00, 0x00, 0x00, 0x2F, 0x00, 0x00,				//	002)	0x21=033	!
0x00, 0x00, 0x07, 0x00, 0x07, 0x00,				//	003)	0x22=034	"
0x00, 0x14, 0x7F, 0x14, 0x7F, 0x14,				//	004)	0x23=035	#
0x00, 0x24, 0x2A, 0x7F, 0x2A, 0x12,				//	005)	0x24=036	$
0x00, 0x23, 0x13, 0x08, 0x64, 0x62,				//	006)	0x25=037	%
0x00, 0x36, 0x49, 0x55, 0x22, 0x50,				//	007)	0x26=038	&
0x00, 0x00, 0x05, 0x03, 0x00, 0x00,				//	008)	0x27=039	'
0x00, 0x00, 0x1C, 0x22, 0x41, 0x00,				//	009)	0x28=040	(
0x00, 0x00, 0x41, 0x22, 0x1C, 0x00,				//	010)	0x29=041	)
0x00, 0x14, 0x08, 0x3E, 0x08, 0x14,				//	011)	0x2A=042	*
0x00, 0x08, 0x08, 0x3E, 0x08, 0x08,				//	012)	0x2B=043	+
0x00, 0x00, 0x00, 0xA0, 0x60, 0x00,				//	013)	0x2C=044	,
0x00, 0x08, 0x08, 0x08, 0x08, 0x08,				//	014)	0x2D=045	-
0x00, 0x00, 0x60, 0x60, 0x00, 0x00,				//	015)	0x2E=046	.
0x00, 0x20, 0x10, 0x08, 0x04, 0x02,				//	016)	0x2F=047	/
		/*										//
0x00, 0x3E, 0x51, 0x49, 0x45, 0x3E,				//	017)	0x30=048	0
0x00, 0x00, 0x42, 0x7F, 0x40, 0x00,				//	018)	0x31=049	1
0x00, 0x42, 0x61, 0x51, 0x49, 0x46,				//	019)	0x32=050	2
0x00, 0x21, 0x41, 0x45, 0x4B, 0x31,				//	020)	0x33=051	3
0x00, 0x18, 0x14, 0x12, 0x7F, 0x10,				//	021)	0x34=052	4
0x00, 0x27, 0x45, 0x45, 0x45, 0x39,				//	022)	0x35=053	5
0x00, 0x3C, 0x4A, 0x49, 0x49, 0x30,				//	023)	0x36=054	6
0x00, 0x01, 0x71, 0x09, 0x05, 0x03,				//	024)	0x37=055	7
0x00, 0x36, 0x49, 0x49, 0x49, 0x36,				//	025)	0x38=056	8
0x00, 0x06, 0x49, 0x49, 0x29, 0x1E,				//	026)	0x39=057	9
0x00, 0x00, 0x36, 0x36, 0x00, 0x00,				//	027)	0x3A=058	:
0x00, 0x00, 0x56, 0x36, 0x00, 0x00,				//	028)	0x3B=059	;
0x00, 0x08, 0x14, 0x22, 0x41, 0x00,				//	029)	0x3C=060	<
0x00, 0x14, 0x14, 0x14, 0x14, 0x14,				//	030)	0x3D=061	=
0x00, 0x00, 0x41, 0x22, 0x14, 0x08,				//	031)	0x3E=062	>
0x00, 0x02, 0x01, 0x51, 0x09, 0x06,				//	032)	0x3F=063	?
												//
0x00, 0x32, 0x49, 0x59, 0x51, 0x3E,				//	033)	0x40=064	@
0x00, 0x7C, 0x12, 0x11, 0x12, 0x7C,				//	034)	0x41=065	A
0x00, 0x7F, 0x49, 0x49, 0x49, 0x36,				//	035)	0x42=066	B
0x00, 0x3E, 0x41, 0x41, 0x41, 0x22,				//	036)	0x43=067	C
0x00, 0x7F, 0x41, 0x41, 0x22, 0x1C,				//	037)	0x44=068	D
0x00, 0x7F, 0x49, 0x49, 0x49, 0x41,				//	038)	0x45=069	E
0x00, 0x7F, 0x09, 0x09, 0x09, 0x01,				//	039)	0x46=070	F
0x00, 0x3E, 0x41, 0x49, 0x49, 0x7A,				//	040)	0x47=071	G
0x00, 0x7F, 0x08, 0x08, 0x08, 0x7F,				//	041)	0x48=072	H
0x00, 0x00, 0x41, 0x7F, 0x41, 0x00,				//	042)	0x49=073	I
0x00, 0x20, 0x40, 0x41, 0x3F, 0x01,				//	043)	0x4A=074	J
0x00, 0x7F, 0x08, 0x14, 0x22, 0x41,				//	044)	0x4B=075	K
0x00, 0x7F, 0x40, 0x40, 0x40, 0x40,				//	045)	0x4C=076	L
0x00, 0x7F, 0x02, 0x0C, 0x02, 0x7F,				//	046)	0x4D=077	M
0x00, 0x7F, 0x04, 0x08, 0x10, 0x7F,				//	047)	0x4E=078	N
0x00, 0x3E, 0x41, 0x41, 0x41, 0x3E,				//	048)	0x4F=079	O
												//
0x00, 0x7F, 0x09, 0x09, 0x09, 0x06,				//	049)	0x50=080	P
0x00, 0x3E, 0x41, 0x51, 0x21, 0x5E,				//	050)	0x51=081	Q
0x00, 0x7F, 0x09, 0x19, 0x29, 0x46,				//	051)	0x52=082	R
0x00, 0x46, 0x49, 0x49, 0x49, 0x31,				//	052)	0x53=083	S
0x00, 0x01, 0x01, 0x7F, 0x01, 0x01,				//	053)	0x54=084	T
0x00, 0x3F, 0x40, 0x40, 0x40, 0x3F,				//	054)	0x55=085	U
0x00, 0x1F, 0x20, 0x40, 0x20, 0x1F,				//	055)	0x56=086	V
0x00, 0x3F, 0x40, 0x38, 0x40, 0x3F,				//	056)	0x57=087	W
0x00, 0x63, 0x14, 0x08, 0x14, 0x63,				//	057)	0x58=088	X
0x00, 0x07, 0x08, 0x70, 0x08, 0x07,				//	058)	0x59=089	Y
0x00, 0x61, 0x51, 0x49, 0x45, 0x43,				//	059)	0x5A=090	Z
0x00, 0x00, 0x7F, 0x41, 0x41, 0x00,				//	060)	0x5B=091	[
0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55,				//	061)	0x5C=092	обратный слеш
0x00, 0x00, 0x41, 0x41, 0x7F, 0x00,				//	062)	0x5D=093	]
0x00, 0x04, 0x02, 0x01, 0x02, 0x04,				//	063)	0x5E=094	^
0x00, 0x40, 0x40, 0x40, 0x40, 0x40,				//	064)	0x5F=095	_
												//
0x00, 0x00, 0x03, 0x05, 0x00, 0x00,				//	065)	0x60=096	`
0x00, 0x20, 0x54, 0x54, 0x54, 0x78,				//	066)	0x61=097	a
0x00, 0x7F, 0x28, 0x44, 0x44, 0x38,				//	067)	0x62=098	b
0x00, 0x38, 0x44, 0x44, 0x44, 0x20,				//	068)	0x63=099	c
0x00, 0x38, 0x44, 0x44, 0x48, 0x7F,				//	069)	0x64=100	d
0x00, 0x38, 0x54, 0x54, 0x54, 0x18,				//	070)	0x65=101	e
0x00, 0x08, 0x7E, 0x09, 0x01, 0x02,				//	071)	0x66=102	f
0x00, 0x18, 0xA4, 0xA4, 0xA4, 0x7C,				//	072)	0x67=103	g
0x00, 0x7F, 0x08, 0x04, 0x04, 0x78,				//	073)	0x68=104	h
0x00, 0x00, 0x44, 0x7D, 0x40, 0x00,				//	074)	0x69=105	i
0x00, 0x40, 0x80, 0x84, 0x7D, 0x00,				//	075)	0x6A=106	j
0x00, 0x7F, 0x10, 0x28, 0x44, 0x00,				//	076)	0x6B=107	k
0x00, 0x00, 0x41, 0x7F, 0x40, 0x00,				//	077)	0x6C=108	l
0x00, 0x7C, 0x04, 0x18, 0x04, 0x78,				//	078)	0x6D=109	m
0x00, 0x7C, 0x08, 0x04, 0x04, 0x78,				//	079)	0x6E=110	n
0x00, 0x38, 0x44, 0x44, 0x44, 0x38,				//	080)	0x6F=111	o
												//
0x00, 0xFC, 0x24, 0x24, 0x24, 0x18,				//	081)	0x70=112	p
0x00, 0x18, 0x24, 0x24, 0x18, 0xFC,				//	082)	0x71=113	q
0x00, 0x7C, 0x08, 0x04, 0x04, 0x08,				//	083)	0x72=114	r
0x00, 0x48, 0x54, 0x54, 0x54, 0x20,				//	084)	0x73=115	s
0x00, 0x04, 0x3F, 0x44, 0x40, 0x20,				//	085)	0x74=116	t
0x00, 0x3C, 0x40, 0x40, 0x20, 0x7C,				//	086)	0x75=117	u
0x00, 0x1C, 0x20, 0x40, 0x20, 0x1C,				//	087)	0x76=118	v
0x00, 0x3C, 0x40, 0x30, 0x40, 0x3C,				//	088)	0x77=119	w
0x00, 0x44, 0x28, 0x10, 0x28, 0x44,				//	089)	0x78=120	x
0x00, 0x1C, 0xA0, 0xA0, 0xA0, 0x7C,				//	090)	0x79=121	y
0x00, 0x44, 0x64, 0x54, 0x4C, 0x44,				//	091)	0x7A=122	z
0x00, 0x00, 0x10, 0x7C, 0x82, 0x00,				//	092)	0x7B=123	{
0x00, 0x00, 0x00, 0xFF, 0x00, 0x00,				//	093)	0x7C=124	|
0x00, 0x00, 0x82, 0x7C, 0x10, 0x00,				//	094)	0x7D=125	}
0x00, 0x00, 0x06, 0x09, 0x09, 0x06,				//	095)	0x7E=126	~
												//
0												//	В таблице имеется (0) пустых интервалов. См. шрифты с Русскими символами.*/
};					

const uint8_t BigNumbers[] =																																																								//	Шрифт	BigNumbers
{																																																																//
0x0e, 0x18, 0x2d, 0x0d,				
	//	ширина символов (14), высота символов (24), код первого символа (45), количество символов (13)
	
	
0x00, 0xfc, 0xfa, 0xf6, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x06, 0x02, 0x00, 0x00, 0x00, 0x0f, 0x17, 0x3b, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0xb8, 0xd0, 0xe0, 0x00, 0x00, 0x00, 0x80, 0xc0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xdf, 0xbf, 0x7f, 0x00,		//	009)	0x35=053	5
	
	
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		//	001)	0x2D=045	-
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0xe0, 0xe0, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,		//	002)	0x2E=046	.
0x00, 0x00, 0x02, 0x06, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x06, 0x02, 0x00, 0x00, 0x00, 0x00, 0x10, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x10, 0x00, 0x00, 0x00, 0x00, 0x80, 0xc0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xc0, 0x80, 0x00, 0x00,		//	003)	0x2F=047	/
0x00, 0xfc, 0xfa, 0xf6, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0xf6, 0xfa, 0xfc, 0x00, 0x00, 0xef, 0xc7, 0x83, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x83, 0xc7, 0xef, 0x00, 0x00, 0x7f, 0xbf, 0xdf, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xdf, 0xbf, 0x7f, 0x00,		//	004)	0x30=048	0
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xf8, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x83, 0xc7, 0xef, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x3f, 0x7f, 0x00,		//	005)	0x31=049	1
0x00, 0x00, 0x02, 0x06, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0xf6, 0xfa, 0xfc, 0x00, 0x00, 0xe0, 0xd0, 0xb8, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x3b, 0x17, 0x0f, 0x00, 0x00, 0x7f, 0xbf, 0xdf, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xc0, 0x80, 0x00, 0x00,		//	006)	0x32=050	2
0x00, 0x00, 0x02, 0x06, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0xf6, 0xfa, 0xfc, 0x00, 0x00, 0x00, 0x10, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0xbb, 0xd7, 0xef, 0x00, 0x00, 0x00, 0x80, 0xc0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xdf, 0xbf, 0x7f, 0x00,		//	007)	0x33=051	3
0x00, 0xfc, 0xf8, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xf8, 0xfc, 0x00, 0x00, 0x0f, 0x17, 0x3b, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0xbb, 0xd7, 0xef, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x3f, 0x7f, 0x00,		//	008)	0x34=052	4
0x00, 0xfc, 0xfa, 0xf6, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x06, 0x02, 0x00, 0x00, 0x00, 0x0f, 0x17, 0x3b, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0xb8, 0xd0, 0xe0, 0x00, 0x00, 0x00, 0x80, 0xc0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xdf, 0xbf, 0x7f, 0x00,		//	009)	0x35=053	5
0x00, 0xfc, 0xfa, 0xf6, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x06, 0x02, 0x00, 0x00, 0x00, 0xef, 0xd7, 0xbb, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0xb8, 0xd0, 0xe0, 0x00, 0x00, 0x7f, 0xbf, 0xdf, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xdf, 0xbf, 0x7f, 0x00,		//	010)	0x36=054	6
0x00, 0x00, 0x02, 0x06, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0xf6, 0xfa, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x83, 0xc7, 0xef, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x3f, 0x7f, 0x00,		//	011)	0x37=055	7
0x00, 0xfc, 0xfa, 0xf6, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0xf6, 0xfa, 0xfc, 0x00, 0x00, 0xef, 0xd7, 0xbb, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0xbb, 0xd7, 0xef, 0x00, 0x00, 0x7f, 0xbf, 0xdf, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xdf, 0xbf, 0x7f, 0x00,		//	012)	0x38=056	8
0x00, 0xfc, 0xfa, 0xf6, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0x0e, 0xf6, 0xfa, 0xfc, 0x00, 0x00, 0x0f, 0x17, 0x3b, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0xbb, 0xd7, 0xef, 0x00, 0x00, 0x00, 0x80, 0xc0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xdf, 0xbf, 0x7f, 0x00,		//	013)	0x39=057	9
																																																																//
0																																																																//	В таблице имеется (0) пустых интервалов. См. шрифты с Русскими символами.
};			

const uint8_t site[] =	
{
	7, 8, 0, 0,
	0x00,0x30,0x38,0x3C
};

#define bit_val(buff,offset) ((buff[(offset)>>3]>>((offset)&0x7))&1)

void myChar_(char ch, const uint8_t * buff)
{
	
}

uint8_t GetProportion(uint8_t x, uint8_t dest, uint8_t orig)	// Число. Требуемый размер. Родной размер
{
	uint32_t res = (x<<8) * (orig<<8) / (dest <<8);
	return res >> 8;
}

void myChar(char ch, const uint8_t * buff, uint8_t dest_w, uint8_t dest_h)
{
	//#define 
	//0x06, 0x08, 0x20, 0x5F,							//	ширина символов (6), высота символов (8), код первого символа (32), количество символов (95)
	uint8_t w = buff[0];
	uint8_t h = buff[1];
	if(dest_h == 0) dest_h = h;
	if(dest_w == 0) dest_w = w;
	uint8_t firstCh = buff[2];
	uint8_t num = buff[3];	
	const uint8_t*font = &buff[4];// + (); смещениие вычислить
	
	uint8_t currByte, currBit, color;
	for(int xx=0; xx<dest_w; xx++)
		for(int yy=0; yy<dest_h; yy++)
		{
			uint8_t x = GetProportion(xx, dest_w, w);
			uint8_t y = GetProportion(yy, dest_h, h);
			
			currByte = font[((y>>3) * w) + x];
			currBit = y & 7;
			color = (currByte >> currBit) & 1;
			ssd1306_DrawPixel(xx, yy, color);
		}
	
	return;
	
}






void tmp(void)
{
	myChar(5, SmallFont,12,16);
	ssd1306_UpdateScreen();
	
	myChar(4, site,0,0);
	ssd1306_UpdateScreen();
	
	myChar(5, BigNumbers,8,8);
	ssd1306_UpdateScreen();
	return;
	for(int y=0; y<SSD1306_HEIGHT; y++)
		for(int x =0; x<SSD1306_WIDTH; x++)
		{
			ssd1306_DrawPixel(x, y, White);
			ssd1306_UpdateScreen();
		}
}

char ssd1306_WriteChar(char ch, SSD1306_Font_t Font, SSD1306_COLOR color) {
    uint32_t i, b, j;
    
    // Check if character is valid
    if (ch < 32 || ch > 126)
        return 0;
    
    // Char width is not equal to font width for proportional font
    const uint8_t char_width = Font.char_width ? Font.char_width[ch-32] : Font.width;
    // Check remaining space on current line
    if (SSD1306_WIDTH < (SSD1306.CurrentX + char_width) ||
        SSD1306_HEIGHT < (SSD1306.CurrentY + Font.height))
    {
        // Not enough space on current line
        return 0;
    }
    
    // Use the font to write
    for(i = 0; i < Font.height; i++) {
        b = Font.data[(ch - 32) * Font.height + i];
        for(j = 0; j < char_width; j++) {
            if((b << j) & 0x8000)  {
                ssd1306_DrawPixel(SSD1306.CurrentX + j, (SSD1306.CurrentY + i), (SSD1306_COLOR) color);
            } else {
                ssd1306_DrawPixel(SSD1306.CurrentX + j, (SSD1306.CurrentY + i), (SSD1306_COLOR)!color);
            }
        }
    }
    
    // The current space is now taken
    SSD1306.CurrentX += char_width;
    
    // Return written char for validation
    return ch;
}

/* Write full string to screenbuffer */
char ssd1306_WriteString(char* str, SSD1306_Font_t Font, SSD1306_COLOR color) {
    while (*str) {
        if (ssd1306_WriteChar(*str, Font, color) != *str) {
            // Char could not be written
            return *str;
        }
        str++;
    }
    
    // Everything ok
    return *str;
}

/* Position the cursor */
void ssd1306_SetCursor(uint8_t x, uint8_t y) {
    SSD1306.CurrentX = x;
    SSD1306.CurrentY = y;
}

/* Draw line by Bresenhem's algorithm */
void ssd1306_Line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, SSD1306_COLOR color) {
    int32_t deltaX = abs(x2 - x1);
    int32_t deltaY = abs(y2 - y1);
    int32_t signX = ((x1 < x2) ? 1 : -1);
    int32_t signY = ((y1 < y2) ? 1 : -1);
    int32_t error = deltaX - deltaY;
    int32_t error2;
    
    ssd1306_DrawPixel(x2, y2, color);

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

/* Draw polyline */
void ssd1306_Polyline(const SSD1306_VERTEX *par_vertex, uint16_t par_size, SSD1306_COLOR color) {
    uint16_t i;
    if(par_vertex == NULL) {
        return;
    }

    for(i = 1; i < par_size; i++) {
        ssd1306_Line(par_vertex[i - 1].x, par_vertex[i - 1].y, par_vertex[i].x, par_vertex[i].y, color);
    }

    return;
}

/* Convert Degrees to Radians */
static float ssd1306_DegToRad(float par_deg) {
    return par_deg * (3.14f / 180.0f);
}

/* Normalize degree to [0;360] */
static uint16_t ssd1306_NormalizeTo0_360(uint16_t par_deg) {
    uint16_t loc_angle;
    if(par_deg <= 360) {
        loc_angle = par_deg;
    } else {
        loc_angle = par_deg % 360;
        loc_angle = (loc_angle ? loc_angle : 360);
    }
    return loc_angle;
}

/*
 * DrawArc. Draw angle is beginning from 4 quart of trigonometric circle (3pi/2)
 * start_angle in degree
 * sweep in degree
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

/*
 * Draw arc with radius line
 * Angle is beginning from 4 quart of trigonometric circle (3pi/2)
 * start_angle: start angle in degree
 * sweep: finish angle in degree
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
    
    // Radius line
    ssd1306_Line(x,y,first_point_x,first_point_y,color);
    ssd1306_Line(x,y,xp2,yp2,color);
    return;
}

/* Draw circle by Bresenhem's algorithm */
void ssd1306_DrawCircle(uint8_t par_x,uint8_t par_y,uint8_t par_r,SSD1306_COLOR par_color) {
    int32_t x = -par_r;
    int32_t y = 0;
    int32_t err = 2 - 2 * par_r;
    int32_t e2;

    if (par_x >= SSD1306_WIDTH || par_y >= SSD1306_HEIGHT) {
        return;
    }

    do {
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

/* Draw filled circle. Pixel positions calculated using Bresenham's algorithm */
void ssd1306_FillCircle(uint8_t par_x,uint8_t par_y,uint8_t par_r,SSD1306_COLOR par_color) {
    int32_t x = -par_r;
    int32_t y = 0;
    int32_t err = 2 - 2 * par_r;
    int32_t e2;

    if (par_x >= SSD1306_WIDTH || par_y >= SSD1306_HEIGHT) {
        return;
    }

    do {
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

/* Draw a rectangle */
void ssd1306_DrawRectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, SSD1306_COLOR color) {
    ssd1306_Line(x1,y1,x2,y1,color);
    ssd1306_Line(x2,y1,x2,y2,color);
    ssd1306_Line(x2,y2,x1,y2,color);
    ssd1306_Line(x1,y2,x1,y1,color);

    return;
}

/* Draw a filled rectangle */
void ssd1306_FillRectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, SSD1306_COLOR color) {
    uint8_t x_start = ((x1<=x2) ? x1 : x2);
    uint8_t x_end   = ((x1<=x2) ? x2 : x1);
    uint8_t y_start = ((y1<=y2) ? y1 : y2);
    uint8_t y_end   = ((y1<=y2) ? y2 : y1);

    for (uint8_t y= y_start; (y<= y_end)&&(y<SSD1306_HEIGHT); y++) {
        for (uint8_t x= x_start; (x<= x_end)&&(x<SSD1306_WIDTH); x++) {
            ssd1306_DrawPixel(x, y, color);
        }
    }
    return;
}

SSD1306_Error_t ssd1306_InvertRectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
  if ((x2 >= SSD1306_WIDTH) || (y2 >= SSD1306_HEIGHT)) {
    return SSD1306_ERR;
  }
  if ((x1 > x2) || (y1 > y2)) {
    return SSD1306_ERR;
  }
  uint32_t i;
  if ((y1 / 8) != (y2 / 8)) {
    /* if rectangle doesn't lie on one 8px row */
    for (uint32_t x = x1; x <= x2; x++) {
      i = x + (y1 / 8) * SSD1306_WIDTH;
      SSD1306_Buffer[i] ^= 0xFF << (y1 % 8);
      i += SSD1306_WIDTH;
      for (; i < x + (y2 / 8) * SSD1306_WIDTH; i += SSD1306_WIDTH) {
        SSD1306_Buffer[i] ^= 0xFF;
      }
      SSD1306_Buffer[i] ^= 0xFF >> (7 - (y2 % 8));
    }
  } else {
    /* if rectangle lies on one 8px row */
    const uint8_t mask = (0xFF << (y1 % 8)) & (0xFF >> (7 - (y2 % 8)));
    for (i = x1 + (y1 / 8) * SSD1306_WIDTH;
         i <= (uint32_t)x2 + (y2 / 8) * SSD1306_WIDTH; i++) {
      SSD1306_Buffer[i] ^= mask;
    }
  }
  return SSD1306_OK;
}

/* Draw a bitmap */
void ssd1306_DrawBitmap(uint8_t x, uint8_t y, const unsigned char* bitmap, uint8_t w, uint8_t h, SSD1306_COLOR color) {
    int16_t byteWidth = (w + 7) / 8; // Bitmap scanline pad = whole byte
    uint8_t byte = 0;

    if (x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT) {
        return;
    }

    for (uint8_t j = 0; j < h; j++, y++) {
        for (uint8_t i = 0; i < w; i++) {
            if (i & 7) {
                byte <<= 1;
            } else {
                byte = (*(const unsigned char *)(&bitmap[j * byteWidth + i / 8]));
            }

            if (byte & 0x80) {
                ssd1306_DrawPixel(x + i, y, color);
            }
        }
    }
    return;
}

void ssd1306_SetContrast(const uint8_t value) {
    const uint8_t kSetContrastControlRegister = 0x81;
    ssd1306_WriteCommand(kSetContrastControlRegister);
    ssd1306_WriteCommand(value);
}

void ssd1306_SetDisplayOn(const uint8_t on) {
    uint8_t value;
    if (on) {
        value = 0xAF;   // Display on
        SSD1306.DisplayOn = 1;
    } else {
        value = 0xAE;   // Display off
        SSD1306.DisplayOn = 0;
    }
    ssd1306_WriteCommand(value);
}

uint8_t ssd1306_GetDisplayOn() {
    return SSD1306.DisplayOn;
}
