#ifndef __MISC_H__
#define __MISC_H__

#include <stddef.h>
#include <stdint.h>
// ================== DEFINES ==================


// Расчет напряжения из АЦП на основе значения АЦП референсного значения.(внутренний Vref в STM) 

#define GET_ADC_K(Vref_mV, ADCref) ( ((uint32_t)(Vref_mV)<<19) / ((uint32_t)(ADCref)) ) // коэфициент для вычисления mV из ADC. Vref_mV<=11бит поэтому при сдвиге влезем. ADCref не менее 10 бит, поэтому К не более 20 бита
#define GET_mV(ADC_val, ADC_K) ( ((uint32_t)(ADC_val) * (uint32_t)(ADC_K)) >> 19 ) // Коэфициент вычисляется выше. 12 бит ADC_val и 20 бит ADC_K - влезаем.

// ================== FUNCTIONS ==================

// Поиск медианы массива. Массив сортируется, тоесть изменяется!
uint16_t GetMedian_16(uint16_t * const arr, int n);

// Число в строку. Указатель на выделенную строку(int = 32 бита, значит 11+1 символов), куда поместим результат с выравниванием вправо.
int32_t Int_to_str(int32_t var, char str[12]);

// Таймеры - отслеживание периодов времени
typedef struct
{
	uint32_t timeStamp;
	uint32_t period;
} timer_t;

static inline void Timer_set(timer_t *tim, uint32_t current_time, uint32_t period)
{
	tim->timeStamp = current_time;
	tim->period = period;
}

static inline uint8_t Timer_isExpired(timer_t *tim, uint32_t current_time)
{
	if (tim->period == 0) return 0; // не взведён или остановлен
	if (current_time - tim->timeStamp >= tim->period)
	{
		tim->timeStamp += tim->period;
		return 1;
	}
	return 0;
}




#endif //__MISC_H__
