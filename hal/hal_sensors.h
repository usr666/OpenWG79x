#ifndef _HAL_SENSORS_H
#define _HAL_SENSORS_H
#include <stdbool.h>

typedef enum {
    SENSOR_FRONT=0, SENSOR_LIFT, SENSOR_STOPBTN, SENSOR_RIGHT_WIRE_INSIDE, SENSOR_LEFT_WIRE_INSIDE, SENSOR_NEAR_WIRE, SENSOR_NUMBER_OF_SENSORS
} sensors_t;

void init_hal_sensors(void);
void task_sensors(void);
bool get_sensor(sensors_t sensor);

void trigger_wire_sensor(void);

#define NUMBER_OF_DEBUG_WIRE_TIMES 10
extern uint32_t debug_wire_times[NUMBER_OF_DEBUG_WIRE_TIMES];
extern uint8_t debug_wire_values[NUMBER_OF_DEBUG_WIRE_TIMES];
extern uint8_t debug_wire_idx;
void wire_sensor_debug(bool debug_enable, bool near_range, bool restart_samples);


#endif