#ifndef _SYSTEM_H
#define _SYSTEM_H

#include <stdint.h>

void delay_system_ticks(uint32_t sys_ticks);	
void delay_micro_seconds(uint32_t us);

#define SYS_TICK_PERIOD_IN_MS 10
extern uint32_t systick_cnt; // Counter that is increased every SYS_TICK_PERIOD_IN_MS

#endif