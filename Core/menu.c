#include "menu.h"
#include "main.h"

#define LINE_NUM 4 // количество линий на экране
#define SYMBOL_NUM (128/5) // количество символов в строке

// --- прототипы ---
static void Menu_rout(menu_button_t button); // хождение по меню
static void Menu_main_rout(menu_button_t button);

// --- main menu ---
const item_t main_menu_items[] = { 
								{"Clock", NULL},
								{"LED", NULL}, 
								{"Voltage", NULL},
								{"Temperature", NULL},
								{"Exit", NULL}
								};
const menu_t menu_main = {sizeof(menu_main_items)/sizeof(main_menu_items[0]), menu_main_items, Menu_main_rout};								

// --- clock menu ---
static struct 
{	int hours;
	int minutes;
	int reset;
	int save;
} clock_var;




	// --- clock menu ---
int menu_clock_hours, menu_clock_minutes, menu_clock_reset, menu_clock_save;
const item_t clock_menu_items[] = { 
									{"Hours", &menu_clock_hours},
									{"Minutes", &menu_clock_minutes},
									{"RESET", &menu_clock_reset},
									{"Save", &menu_clock_save},
									{"Back", NULL}
									};
const menu_t menu_clock = {sizeof(clock_menu_items)/sizeof(clock_menu_items[0]), clock_menu_items, Menu_clock_rout};

static const menu_t * current_menu = &menu_main;
static uint8_t current_item_idx = 0;

static void Update_screen()
{
	char screen[LINE_NUM][SYMBOL_NUM+1];
	static uint8_t start_line_idx = 0; // индекс current_item_idx для нулевой строки
	
}

static void Menu_rout(menu_button_t button)
{
	int res; 
	switch (button)
	{
		case MENU_BUTTON_UP:
			if (current_item_idx != 0) current_item_idx--;
			break;
		case MENU_BUTTON_DOWN:
			if (current_item_idx < (current_menu->items_n - 1)) current_item_idx++;
			break;
		case MENU_BUTTON_ENTER:
			res = current_menu->items[current_item_idx]->rout(button);
			break;
		case MENU_BUTTON_GET_VAL:
		default:
			// при прорисовку запросим
			break;		
	}
	// обновляем экран
	Update_screen();
	
}

static void Menu_main_event(menu_button_t button)
{
	switch (button)
	{
		case MENU_BUTTON_UP:
		case MENU_BUTTON_DOWN:
		case MENU_BUTTON_ENTER:
		case MENU_BUTTON_GET_VAL:
	}
	
}

void Menu_clock_event(menu_button_t button)
{
}
