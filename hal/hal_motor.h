#ifndef _HAL_MOTOR_H
#define _HAL_MOTOR_H
#include <stdbool.h>
#include <stdint.h>

#define STEPS_PER_M 211

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
int32_t get_motor_distance(motors_t motor);
int32_t get_motor_speed(motors_t motor);

#endif