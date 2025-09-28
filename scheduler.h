#ifndef _SCHEDULER_H
#define _SCHEDULER_H

void init_scheduler(void);
void task_scheduler(void);
bool in_schedule_time(void);
uint8_t get_current_hour(void);
uint8_t get_current_minute(void);
void set_current_hour(uint8_t v);
void set_current_minute(uint8_t v);

extern bool schedule_active;
extern uint8_t schedule_starttime;
extern uint8_t schedule_endtime;

#endif