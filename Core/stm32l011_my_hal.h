/*
 * stm32l011_my_hal.h
 * Реализация функций, которых почему-то нет в LL
 *  Created on: Oct 7, 2025
 *      Author: myushkov
 */

#ifndef STM32L011_MY_HAL_H_
#define STM32L011_MY_HAL_H_

#include <stddef.h>
#include <stdint.h>
#include "stm32l0xx_ll_i2c.h"
#include "stm32l0xx_ll_adc.h"
#include "stm32l0xx_ll_utils.h"
#include "misc.h"
// ===== I2C =====

int i2c_write(I2C_TypeDef *I2Cx, uint8_t addr, uint8_t *reg, uint16_t reg_size, uint8_t *buff, uint16_t size);
int i2c_read(I2C_TypeDef *I2Cx, uint8_t addr, uint8_t *reg, uint16_t reg_size, uint8_t *buff, uint16_t size);

// ===== EEPROM =====

uint8_t  EEPROM_ReadByte(uint32_t offset);
uint16_t EEPROM_ReadHalfWord(uint32_t offset);
uint32_t EEPROM_ReadWord(uint32_t offset);
void EEPROM_Write_data(uint32_t offset, uint8_t* data, int size);
void EEPROM_WriteByte(uint32_t offset, uint8_t data);
void EEPROM_WriteHalfWord(uint32_t offset, uint16_t data);
void EEPROM_WriteWord(uint32_t offset, uint32_t data);

// ===== ADC =====

uint16_t Read_ADC_Channel(uint32_t channel);
#define ADC_V_REF_mV	1224	// Значение Vref у этого МК

// ===== SysTick =====

extern volatile uint32_t timer_ms;
void timer_ms_init();	// пока для tim2
static inline uint32_t timer_ms_get_counter() {return timer_ms;}

void SetOptionBytes_For_FlashBoot(void);

#endif /* STM32L011_MY_HAL_H_ */
