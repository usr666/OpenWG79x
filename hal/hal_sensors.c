#include "hal_mcu.h"
#include "hal_sensors.h"
#include "system.h"
#include <stdbool.h>

#define FRONT_SENSOR_PORTNO   4
#define FRONT_SENSOR_PINNO    29
#define LIFT_SENSOR_PORTNO    1
#define LIFT_SENSOR_PINNO     16

void init_hal_sensors(void) {
    // Pins are already inputs, seems to work ok

}

bool get_sensor(sensors_t sensor)
{
    uint32_t value;
    if(sensor == SENSOR_FRONT) {
        value = (((LPC_GPIOx(FRONT_SENSOR_PORTNO)->FIOPIN) & (1 << FRONT_SENSOR_PINNO)));
    } else {
        // SENSOR_LIFT
        value = (((LPC_GPIOx(LIFT_SENSOR_PORTNO)->FIOPIN) & (1 << LIFT_SENSOR_PINNO)));
    }
    
    if(value == 0) {
        return true;
    } else {
        return false;
    }
}
