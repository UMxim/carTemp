#include "rtc_DS3231.h"

int DS3231_Read(DS3231_t *time)
{
	uint8_t reg = 0;
	return DS3231_I2C_READ(reg, (uint8_t*)time, sizeof(DS3231_t));
}

int DS3231_Write(DS3231_t *time)
{
	uint8_t reg = 0;
	return DS3231_I2C_WRITE(reg, (uint8_t *)time, sizeof(DS3231_t));
}

int DS3231_Write_byte(uint8_t reg, uint8_t byte)
{
	return DS3231_I2C_WRITE(reg, &byte, 1);
}

static inline uint8_t bcd2bin(uint8_t ones, uint8_t tens)
{
    return tens * 10 + ones;
}

uint32_t DS3231_date_to_sec(const DS3231_t * const time)
{
    uint8_t sec  = bcd2bin(time->seconds, time->seconds_10);
    uint8_t min  = bcd2bin(time->minutes, time->minutes_10);
    uint8_t hour = bcd2bin(time->hours, time->hours_10);

    uint8_t day  = bcd2bin(time->date, time->date_10);
    uint8_t mon  = bcd2bin(time->month, time->month_10);
    uint8_t year = bcd2bin(time->year, time->year_10);

    static const uint16_t days_before_month[] =
    {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };

    uint32_t days = (uint32_t)year * 365U + ((uint32_t)(year + 3U) >> 2);
    days += days_before_month[mon - 1];
    if (mon > 2 && (year & 3U) == 0)
    {
        days++;
    }
    days += (uint32_t)day - 1U;

    return days * 86400UL + (uint32_t)hour * 3600UL + (uint32_t)min * 60UL + (uint32_t)sec;
}

int DS3231_correct(uint8_t new_hour, uint8_t new_minutes, uint8_t new_seconds, uint32_t *last_correct_sec)
{
	DS3231_t time;
	int res = DS3231_Read(&time);
	if (res <= 0)
		return res;
	uint32_t sec_new = new_hour * 3600 + new_minutes * 60 + new_seconds;
	uint32_t sec_timer = bcd2bin(time.seconds, time.seconds_10) + 60 * bcd2bin(time.minutes, time.minutes_10) + 3600 * bcd2bin(time.hours, time.hours_10);

	int32_t delta = (int32_t)sec_new - (int32_t)sec_timer;
	    if (delta > 43200) delta -= 86400;
	    else if (delta < -43200) delta += 86400;

	int64_t pps = (delta * 10000000) / (DS3231_date_to_sec(&time) - *last_correct_sec); // В десятых долях pps
	time.hours_10 = new_hour/10;
	time.hours = new_hour % 10;
	time.minutes_10 = new_minutes / 10;
	time.minutes = new_minutes % 10;
	time.seconds_10 = new_seconds / 10;
	time.seconds = new_seconds % 10;
	time.conv = 1;
	if ( (pps > 127) || (pps < -128) ) ;
	else time.aging_offset = (uint8_t)pps;
	res = DS3231_Write(&time);
	*last_correct_sec = DS3231_date_to_sec(&time);
	return res;
}
