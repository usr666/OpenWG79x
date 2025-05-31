#ifndef _HAL_SENSORS_H
#define _HAL_SENSORS_H
#include <stdbool.h>

typedef enum {
    SENSOR_FRONT=0, SENSOR_LIFT, SENSOR_NUMBER_OF_SENSORS
} sensors_t;

void init_hal_sensors(void);
bool get_sensor(sensors_t sensor);

void EnableTimeMeasure(void);

#define NO_OF_INTERRUPTDATA 10
typedef struct {
    uint32_t time;
    uint8_t intstatus;
}interruptdata_t;
extern interruptdata_t interruptdata[NO_OF_INTERRUPTDATA];

#endif