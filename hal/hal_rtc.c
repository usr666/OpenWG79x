#include "hal_rtc.h"
#include <LPC17xx.h>

void init_rtc(void) {
    LPC_RTC->CALIBRATION = 0x00000000UL;
    LPC_RTC->CCR = 0x11;
}

uint8_t get_rtc_hour(void) {
    return LPC_RTC->HOUR;
}

uint8_t get_rtc_minute(void) {
    return LPC_RTC->MIN;
}

void set_rtc_hour(uint8_t h) {
    LPC_RTC->CCR = 0; // Disable RTC
    LPC_RTC->HOUR = h;
    LPC_RTC->CCR |= 0x11; // Enable RTC
}

void set_rtc_minute(uint8_t m) {
    LPC_RTC->CCR = 0; // Disable RTC
    LPC_RTC->MIN = m;
    LPC_RTC->CCR |= 0x11; // Enable RTC
}

uint8_t get_rtc_second(void) {
    return LPC_RTC->SEC;
}