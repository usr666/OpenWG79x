#ifndef _SYSTEM_H
#define _SYSTEM_H

#include <stdint.h>
#include <stdbool.h>

void delay_system_ticks(uint32_t sys_ticks);	
void delay_micro_seconds(uint32_t us);

typedef struct {
    uint32_t starttime_systick;
    uint32_t timeout_systick;
} systimer_t;
void systimer_start(systimer_t *timer, uint32_t timeout_ms);
bool systimer_is_expired(systimer_t *timer);
uint32_t systimer_get_time_since_started(systimer_t *timer);
uint32_t systimer_get_time_left(systimer_t *timer);

#define SYS_TICK_PERIOD_IN_MS 10
volatile extern uint32_t systick_cnt; // Counter that is increased every SYS_TICK_PERIOD_IN_MS, wraps around after 497 days (will never happen)

//todo: implement these
#define DOASSERT(x)
#define ASSERT(x)

#endif