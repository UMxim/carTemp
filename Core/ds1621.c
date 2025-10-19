/*
 * ds1621.c
 *
 *  Created on: Oct 14, 2025
 *      Author: myushkov
 */

#include "ds1621.h"

#define DS1621_CFG		0xAC
#define DS1621_START	0xEE
#define DS1621_STOP		0x22
#define DS1621_READ		0xAA
#define DS1621_TH		0xA1
#define DS1621_TL		0xA2


static int ds1621_i2c_write(uint8_t reg, uint8_t *buff, uint16_t size)
{
	return i2c_write(DS1621_I2C, DS1621_I2C_ADDR, &reg, 1, buff, size);
}

static int ds1621_i2c_read(uint8_t reg, uint8_t *buff, uint16_t size)
{
	return i2c_read(DS1621_I2C, DS1621_I2C_ADDR, &reg, sizeof(reg), buff, size);
}



int DS1621_get_cfg(ds1621_cfg_t* cfg)
{
	return ds1621_i2c_read(DS1621_CFG, (uint8_t*)cfg, sizeof(ds1621_cfg_t));
}

int DS1621_set_cfg(ds1621_cfg_t* cfg)
{
	return ds1621_i2c_write(DS1621_CFG, (uint8_t*)cfg, sizeof(ds1621_cfg_t));
}


ds1621_temp_t DS1621_get_temp()
{
	ds1621_temp_t temp;
	int res = ds1621_i2c_read(DS1621_READ, (uint8_t*)&temp, sizeof(ds1621_temp_t));
	if (res <= 0) temp.temp = -99;
	return temp;
}

int DS1621_set_thresholds(ds1621_temp_t hi, ds1621_temp_t low)
{
	int res;
	low.temp_05 = low.temp_05 ? 0x80 : 0;
	hi.temp_05 = hi.temp_05 ? 0x80 : 0;
	res = ds1621_i2c_write(DS1621_TH, (uint8_t*)&hi, sizeof(ds1621_temp_t));
	if (res <= 0 ) return res;
	Timer_delay_ms(10);
	res = ds1621_i2c_write(DS1621_TL, (uint8_t*)&low, sizeof(ds1621_temp_t));
	return res;
}

int DS1621_get_thresholds(ds1621_temp_t *hi, ds1621_temp_t *low)
{
	int res;
	res = ds1621_i2c_read(DS1621_TH, (uint8_t*)&hi, sizeof(ds1621_temp_t));
	if (res <= 0 ) return res;
	res = ds1621_i2c_read(DS1621_TL, (uint8_t*)&low, sizeof(ds1621_temp_t));
	return res;
}

int DS1621_start_convert()
{
	return ds1621_i2c_write(DS1621_START, NULL, 0);
}

int DS1621_stop_convert()
{
	return ds1621_i2c_write(DS1621_STOP, NULL, 0);
}
