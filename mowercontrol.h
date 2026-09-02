#ifndef _MOWERCONTROL_H
#define _MOWERCONTROL_H

#include <stdbool.h>
#include <stdint.h>

void init_mowercontrol(bool start_stopped, uint8_t reason_code);
void task_mowercontrol(void);
uint8_t mowercontrol_get_state(void);
bool remotecontrol_run(bool forcerun, int8_t left_speed, int8_t right_speed, int8_t disc_speed);
bool remotecontrol_turn(bool forcerun, int8_t wheel_speed, uint8_t turn_angle, bool turn_right, int8_t disc_speed);

extern bool avoid_downhill;
extern bool sideways_down;
extern uint8_t circlespeed;
extern uint8_t stopreason;
extern bool is_stopped;

#endif