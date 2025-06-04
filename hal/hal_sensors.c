#include "hal_mcu.h"
#include "hal_sensors.h"
#include "system.h"
#include <stdbool.h>

#define FRONT_SENSOR_PORTNO   4
#define FRONT_SENSOR_PINNO    29
#define LIFT_SENSOR_PORTNO    1
#define LIFT_SENSOR_PINNO     16
#define STOPBTN_SENSOR_PORTNO    1
#define STOPBTN_SENSOR_PINNO     17

#define RIGHT_SENSOR_RISING_EDGE_BITVAL 0x200
#define LEFT_SENSOR_RISING_EDGE_BITVAL 0x080
#define RIGHT_SENSOR_FALLING_EDGE_BITVAL 0x400
#define LEFT_SENSOR_FALLING_EDGE_BITVAL 0x100

static bool right_wire_sensor = false, left_wire_sensor = false;
static bool right_firstedge_detected = false, left_firstedge_detected = false;

void init_hal_sensors(void) {
    // Input pins seems to work ok, set outputs for controlling wire sensors
    LPC_GPIO0->FIODIR |= (1<<21 | 1<<22);

    // Setup timer0 for use in wire sensor interrupt time measures
    // Power up Timer0
    LPC_SC->PCONP |= (1 << 1);
    // Timer0 in Timer Mode
    LPC_TIM0->CTCR = 0x0;
    // No prescaler (increment every PCLK tick)
    LPC_TIM0->PR = 0;
    // Start the timer
    LPC_TIM0->TCR = 1;

    // Let triggersensor setup and start interrupt
    NVIC_DisableIRQ(EINT3_IRQn);
}

bool get_sensor(sensors_t sensor)
{
    if(sensor == SENSOR_FRONT) {
        return ((((LPC_GPIOx(FRONT_SENSOR_PORTNO)->FIOPIN) & (1 << FRONT_SENSOR_PINNO))) == 0 ? true : false);
    } else if(sensor == SENSOR_LIFT) {
        return ((((LPC_GPIOx(LIFT_SENSOR_PORTNO)->FIOPIN) & (1 << LIFT_SENSOR_PINNO))) == 0 ? true : false);
    } else if(sensor == SENSOR_STOPBTN) {
        return ((((LPC_GPIOx(STOPBTN_SENSOR_PORTNO)->FIOPIN) & (1 << STOPBTN_SENSOR_PINNO))) == 0 ? false : true);
    } else if(sensor == SENSOR_RIGHT_WIRE_INSIDE) {
        return right_wire_sensor;
    } else if(sensor == SENSOR_LEFT_WIRE_INSIDE) {
        return left_wire_sensor;
    }

    return false; // Should never happen, todo: assert
}

/** \brief  Read External Interrupt Enable status

    The function reads if an interrupt is enabled

    \param [in]      IRQn  External interrupt number. Value cannot be negative.
 */
__STATIC_INLINE bool NVIC_IsIRQEnabled(IRQn_Type IRQn)
{
  return((NVIC->ISER[((uint32_t)(IRQn) >> 5)] & (1 << ((uint32_t)(IRQn) & 0x1F))) > 0);
}

/* 
  Triggers a new read of the wire sensors. Result is available in get_sensor function after TBD ms.
  Until the new sensor data is read last sensor data is returned by get_sensor function.
  If no wire pulses has been detected since last call to trigger_wire_sensor all sensors are set to outside
  since no wire signal is detected. Therefore this function should not be called with a shorter interval 
  than TBD ms.
*/
void trigger_wire_sensor(void)
{
    // Are interrupts still active? This means no interrupts has been triggered since last call to this function. Probably no signal detected.
    if(NVIC_IsIRQEnabled(EINT3_IRQn)) {
        right_wire_sensor = false;
        left_wire_sensor = false;
        return;
    }
    right_firstedge_detected = false;
    left_firstedge_detected = false;

    // Reset timer0
    LPC_TIM0->TCR = 0x02;
    LPC_TIM0->TCR = 0x01;

    LPC_GPIOINT->IO0IntEnR = (RIGHT_SENSOR_RISING_EDGE_BITVAL | LEFT_SENSOR_RISING_EDGE_BITVAL);
    LPC_GPIOINT->IO0IntEnF = (RIGHT_SENSOR_FALLING_EDGE_BITVAL | LEFT_SENSOR_FALLING_EDGE_BITVAL);
    NVIC_EnableIRQ(EINT3_IRQn);
}

/* Conclusions of test with wire sensors
     If the first edge after a long time without edges (at least 0x10000 TIMER0-ticks) is a rising edge that sensor is inside the wire
     If the first edge after a long time without edges (at least 0x10000 TIMER0-ticks) is a falling edge that sensor is outside the wire
     Bit 0x200 is Right sensor rising edge
     Bit 0x400 is Right sensor falling edge
     Bit 0x080 is Left sensor rising edge
     Bit 0x100 is Left sensor falling edge
     Not sure what the output pin P0.22 do, but it seem to affect wire sensors in some way...
     P0.21 = 0 works all the time
     P0.21 = 1 works when the sensor is near the wire. Far from wire there are no pulses on the wire sensor signals. In this mode it seems possible to determine which side is closest to the wire.
*/
void __attribute__ ((interrupt)) EINT3_IRQHandler(void)
{
    uint32_t time;
    uint32_t intf, intr;

    LPC_SC->EXTINT = 1<<3; // Clear EINT3 flag

    // Read and reset timer
    time =  LPC_TIM0->TC;
    LPC_TIM0->TCR = 0x02;
    LPC_TIM0->TCR = 0x01;

    // Read and clear interrupt source
    intr = LPC_GPIOINT->IO0IntStatR;
    intf = LPC_GPIOINT->IO0IntStatF;
    LPC_GPIOINT->IO0IntClr = intr;
    LPC_GPIOINT->IO0IntClr = intf;

    #define LONG_WIRESENSOR_IDLE_TIME 0x10000
    if(time > LONG_WIRESENSOR_IDLE_TIME || left_firstedge_detected || right_firstedge_detected)
    {
        if(!left_firstedge_detected) {
            if((intf & LEFT_SENSOR_FALLING_EDGE_BITVAL) > 0) {
                left_firstedge_detected = true;
                left_wire_sensor = false;
            } else if((intr & LEFT_SENSOR_RISING_EDGE_BITVAL) > 0) {
                left_firstedge_detected = true;
                left_wire_sensor = true;
            }
        }
        if(!right_firstedge_detected) {
            if((intf & RIGHT_SENSOR_FALLING_EDGE_BITVAL) > 0) {
                right_firstedge_detected = true;
                right_wire_sensor = false;
            } else if((intr & RIGHT_SENSOR_RISING_EDGE_BITVAL) > 0) {
                right_firstedge_detected = true;
                right_wire_sensor = true;
            }
        }

        if(left_firstedge_detected && right_firstedge_detected)
        {
            NVIC_DisableIRQ(EINT3_IRQn);
            LPC_GPIOINT->IO0IntEnR = 0;
            LPC_GPIOINT->IO0IntEnF = 0;
        }
    }
}