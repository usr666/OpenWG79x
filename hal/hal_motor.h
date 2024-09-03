#ifndef _HAL_MOTOR_H
#define _HAL_MOTOR_H
#include <stdbool.h>

typedef enum {
    MOTOR_RIGHT = 0,
    MOTOR_LEFT,
    MOTOR_SPINDLE,
    MOTOR_NUMBER_OF_MOTORS
}motors_t;

void init_hal_motor(void);
void task_motor(void);
void set_motor_ramp(motors_t motor, uint8_t ramp_percent);
void set_motor_speed(motors_t motor, int8_t speed);

#endif