#ifndef __MENU_H__
#define __MENU_H__

typedef struct
{
	const char * const name;
	int(*const rout)(menu_button_t button); // Обработчик событий
}item_t;

typedef struct 
{
	const uint32_t items_n;	
	const item_t * items;	
} menu_t;

typedef enum 
{	
	MENU_BUTTON_UP,
	MENU_BUTTON_DOWN,
	MENU_BUTTON_ENTER,
	MENU_BUTTON_UPDATE
}menu_button_t ;
#endif //__MENU_H__