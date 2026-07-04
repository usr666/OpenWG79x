#include "system.h"
#include <stdio.h>
#include "scheduler.h"
#include "hal/hal_rtc.h"

bool schedule_active = true; 
uint8_t schedule_starttime=0;
uint8_t schedule_endtime=8;

void init_scheduler(void) 
{
    schedule_active = false;
    schedule_starttime = 8;
    schedule_endtime = 16;
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

    curhour = get_rtc_hour();

    if(schedule_starttime < schedule_endtime) {
        return(curhour >= schedule_starttime && curhour < schedule_endtime);
    } else {
        return(curhour >= schedule_starttime || curhour < schedule_endtime);
    }
}

