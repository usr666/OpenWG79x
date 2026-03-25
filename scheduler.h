#ifndef _SCHEDULER_H
#define _SCHEDULER_H

void init_scheduler(void);
void task_scheduler(void);
bool in_schedule_time(void);

extern bool schedule_active;
extern uint8_t schedule_starttime;
extern uint8_t schedule_endtime;

#endif