#ifndef _HAL_ADC_H
#define _HAL_ADC_H
#include <stdbool.h>
#include <stdint.h>

void init_hal_adc(void);
void task_hal_adc(void);
uint32_t hal_adc_get_value(uint8_t channelno);
#endif