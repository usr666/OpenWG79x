#ifndef _MOWERCONTROL_H
#define _MOWERCONTROL_H

#include <stdbool.h>
#include <stdint.h>

void init_mowercontrol(bool start_stopped, uint8_t reason_code);
void task_mowercontrol(void);

extern bool avoid_downhill;
extern bool sideways_down;
extern uint8_t circlespeed;
extern uint8_t stopreason;
extern bool is_stopped;

#endif