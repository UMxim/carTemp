#include <stdio.h>
#include "misc.h"

static void _Sort(uint16_t *arr, int n)
{
	for (int i = 1; i < n; i++) {
		uint16_t key = arr[i];
        int j = i - 1;
        while (j >= 0 && arr[j] > key) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

// ====== interface ======
uint16_t GetMedian_16(uint16_t *arr, int n)
{
	_Sort(arr, n);
	return arr[n>>1];
}

int32_t Int_to_str(int32_t var, char str[12]) 
{  
	// Буфер для временного хранения строки числа
    char temp[12] ={0,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '};
    int i = 1;
    
    if (var == 0) // Обработка нуля отдельно
	{
        temp[i++] = '0';
    } 
	else 
	{        
        int negative = (var < 0);// Работаем с абсолютным значением
        uint32_t num = (var == -2147483648) ? 2147483648U : (negative ? -var : var);
        
        while (num > 0) // Заполняем temp цифрами в обратном порядке
		{
            temp[i++] = '0' + (num % 10);
            num /= 10;
        }
        
        if (negative) // Добавляем минус, если число отрицательное
		{
            temp[i++] = '-';
        }        
    }

    // Заполняем str: последний в str - первый в temp
    for (int j = 0; j < 12; j++) 
        str[j] = temp[12-j-1];    
    
    return i; // Размер ненулевых элементов
}