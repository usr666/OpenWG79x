#ifndef _HAL_SENSORS_H
#define _HAL_SENSORS_H
#include <stdbool.h>

typedef enum {
    SENSOR_FRONT=0, SENSOR_LIFT, SENSOR_STOPBTN, SENSOR_RIGHT_WIRE_INSIDE, SENSOR_LEFT_WIRE_INSIDE, SENSOR_NUMBER_OF_SENSORS
} sensors_t;

void init_hal_sensors(void);
bool get_sensor(sensors_t sensor);

void trigger_wire_sensor(void);

#endif