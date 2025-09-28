#include "system.h"
#include <stdio.h>
#include "scheduler.h"

#define TICKS_PER_DAY (24*3600*(1000/SYS_TICK_PERIOD_IN_MS))

bool schedule_active = true; 
uint8_t schedule_starttime=0;
uint8_t schedule_endtime=8;

// Start one day later in this module to avoid negative values
uint32_t systick_daystart=TICKS_PER_DAY;
#define local_systick_cnt (systick_cnt+TICKS_PER_DAY)

void init_scheduler(void) 
{
}

void task_scheduler(void)
{
}

bool in_schedule_time(void)
{
    uint8_t curhour;

    if(schedule_active==false) {
        return true; // always within time if schedule is disabled
    }

    curhour = get_current_hour();

    if(schedule_starttime < schedule_endtime) {
        return(curhour >= schedule_starttime && curhour < schedule_endtime);
    } else {
        return(curhour >= schedule_starttime || curhour < schedule_endtime);
    }
}

#define SECONDS_PER_DAY (3600*24)

uint32_t get_seconds_since_midnight(void)
{
    uint32_t day_seconds;
    day_seconds = (local_systick_cnt - systick_daystart) / (1000/SYS_TICK_PERIOD_IN_MS);
    if(day_seconds > SECONDS_PER_DAY) {
        systick_daystart += (SECONDS_PER_DAY * SYS_TICK_PERIOD_IN_MS / 1000);
        day_seconds = (local_systick_cnt - systick_daystart) / (1000/SYS_TICK_PERIOD_IN_MS);
    }
    return(day_seconds);
}

uint8_t get_current_hour(void)
{
    return(get_seconds_since_midnight()/3600);
}

uint8_t get_current_minute(void)
{
    uint32_t s, m;
    s=get_seconds_since_midnight();
    m=s/60;
    return(m%60);
}

void set_current_hour(uint8_t v)
{
    uint8_t currenthour;
    int32_t adj_seconds, adj_ticks;

    currenthour = get_current_hour();
    adj_seconds = (v - currenthour) * 3600;
    adj_ticks = adj_seconds * (1000/SYS_TICK_PERIOD_IN_MS);
    systick_daystart -= adj_ticks;
}

void set_current_minute(uint8_t v)
{
    uint8_t currentminute;
    int32_t adj_seconds, adj_ticks;

    currentminute = get_current_minute();
    adj_seconds = (v - currentminute) * 60;
    adj_ticks = adj_seconds * (1000/SYS_TICK_PERIOD_IN_MS);
    systick_daystart -= adj_ticks;    
}