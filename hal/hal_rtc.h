#ifndef _HAL_RTC_H
#define _HAL_RTC_H

#include <stdint.h>

void init_rtc(void);
uint8_t get_rtc_hour(void);
uint8_t get_rtc_minute(void);
uint8_t get_rtc_second(void);
void set_rtc_hour(uint8_t h);
void set_rtc_minute(uint8_t m);

#endif