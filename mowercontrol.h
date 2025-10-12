#ifndef _MOWERCONTROL_H
#define _MOWERCONTROL_H

void init_mowercontrol(void);
void task_mowercontrol(void);

extern bool avoid_downhill;
extern bool sideways_down;

#endif