/*
 * ds1621.h
 *
 *  Created on: Oct 14, 2025
 *      Author: myushkov
 */

#ifndef DS1621_H_
#define DS1621_H_

#include "misc.h"

// ===== user cfg =====
#include "stm32l011_my_hal.h"
#define DS1621_I2C 						I2C1
#define DS1621_I2C_ADDR					(0x4F<<1)

// ===== end user cfg =====

typedef struct
{
	int8_t  temp;
	uint8_t temp_05; // к результату добавить 0.5С (Если -3С то с этим битом будет -2.5С)

}ds1621_temp_t;

typedef struct
{
	uint8_t _1shot 		:1;
	uint8_t pol			:1; // 1- active hi
	uint8_t reserved	:2;
	uint8_t nvb			:1; // EE memory busy flag
	uint8_t tlf			:1; // Temperature Low Flag
	uint8_t thf			:1; // Temperature High Flag
	uint8_t done		:1; // Conversion Done bit
}ds1621_cfg_t;

int DS1621_get_cfg(ds1621_cfg_t* cfg);
int DS1621_set_cfg(ds1621_cfg_t* cfg);	// после подождать 10мс для записи
ds1621_temp_t DS1621_get_temp();
int DS1621_set_thresholds(ds1621_temp_t hi, ds1621_temp_t low); // после подождать 10мс для записи
int DS1621_get_thresholds(ds1621_temp_t *hi, ds1621_temp_t *low);
int DS1621_start_convert();
int DS1621_stop_convert();

#endif /* DS1621_H_ */
