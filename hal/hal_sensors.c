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

    // Setup timer0 for use in interrupt time measures
    // Power up Timer0
    LPC_SC->PCONP |= (1 << 1);

    // Timer0 in Timer Mode
    LPC_TIM0->CTCR = 0x0;

    // No prescaler (increment every PCLK tick)
    LPC_TIM0->PR = 0;

    // Start the timer
    LPC_TIM0->TCR = 1;
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

interruptdata_t interruptdata[NO_OF_INTERRUPTDATA];
static uint8_t index = 0;
static bool firstrun;

void EnableTimeMeasure(void)
{
    index = 0;
    firstrun = true;
    LPC_GPIOINT->IO0IntEnR = 0x280;
    LPC_GPIOINT->IO0IntEnF = 0x500;
    NVIC_EnableIRQ(EINT3_IRQn);    
}

#define RIGHT_SENSOR_RISING_EDGE_BITVAL 0x200
#define LEFT_SENSOR_RISING_EDGE_BITVAL 0x080
#define RIGHT_SENSOR_FALLING_EDGE_BITVAL 0x400
#define LEFT_SENSOR_FALLING_EDGE_BITVAL 0x100
/* Conclusions of test with wire sensors
     If the first edge after a long time without edges (at least 0x10000 TIMER0-ticks) is a rising edge that sensor is inside the wire
     If the first edge after a long time without edges (at least 0x10000 TIMER0-ticks) is a falling edge that sensor is outside the wire
     Bit 0x200 is Right sensor rising edge
     Bit 0x400 is Right sensor falling edge
     Bit 0x080 is Left sensor rising edge
     Bit 0x100 is Left sensor falling edge
*/
void __attribute__ ((interrupt)) EINT3_IRQHandler(void)
{
    uint32_t time;
    uint32_t intf, intr;

    LPC_SC->EXTINT = 1<<3; // Clear EINT3 flag
    time =  LPC_TIM0->TC;
    // Reset timer0
    LPC_TIM0->TCR = 0x02;
    LPC_TIM0->TCR = 0x01;

    intr = LPC_GPIOINT->IO0IntStatR;
    intf = LPC_GPIOINT->IO0IntStatF;
    LPC_GPIOINT->IO0IntClr = intr;
    LPC_GPIOINT->IO0IntClr = intf;

    if(firstrun) {
        firstrun = false;
        return;
    }

    if(index < NO_OF_INTERRUPTDATA) {
        if(index>0 || time > 100000) {
            interruptdata[index].time = time;
            interruptdata[index].intstatus = ((intf >> 7 ) & 0x0f) | ((intr >> 3 ) & 0xf0);
            index++;
        }
    } else {
        LPC_GPIOINT->IO0IntEnR = 0;
        LPC_GPIOINT->IO0IntEnF = 0;
    }
}