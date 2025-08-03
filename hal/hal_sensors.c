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
#define WIRESENS_RANGE_PORTNO 0
#define WIRESENS_RANGE_PINNO 22
#define WIRESENS_POLARITY_PORTNO 0
#define WIRESENS_POLARITY_PINNO 22

#define RIGHT_SENSOR_RISING_EDGE_BITVAL 0x200
#define LEFT_SENSOR_RISING_EDGE_BITVAL 0x080
#define RIGHT_SENSOR_FALLING_EDGE_BITVAL 0x400
#define LEFT_SENSOR_FALLING_EDGE_BITVAL 0x100

#define LONG_WIRESENSOR_IDLE_TIME 0x40000UL
#define TOO_LONG_WIRESENSOR_IDLE_TIME 0x50000UL
#define MAX_TIME_BETWEEN_FIRSTEDGES 0x200UL
#define MAX_TIME_BEFORE_OTHEREDGE 0x1100UL
#define MAX_FAILED_ATTEMPTS_NEARWIRE 5
#define MAX_FAILED_ATTEMPTS_FARWIRE 10
#define FAILED_ATTEMPTS_BEFORE_TOGGLE_POLARITY 2
#define FIND_SIGNAL_TIMEOUT_MS 30
#define TRY_NEAR_INTERVAL_MS 500


typedef enum {
    wirestate_start_sampling = 0,
    wirestate_near_wire,
    wirestate_far_from_wire
}wirestate_t;
static wirestate_t wirestate;

static bool near_wire = false, valid_signal_detected = false;
static bool right_wire_sensor = false, left_wire_sensor = false;
static bool filtered_near_wire = false, filtered_right_wire_sensor=false, filtered_left_wire_sensor = false;
static int time_between_firstedges; // Can be used to find out which of left and right sensor is closest to wire
static bool debugmode;
uint32_t debug_wire_times[NUMBER_OF_DEBUG_WIRE_TIMES];
uint8_t debug_wire_values[NUMBER_OF_DEBUG_WIRE_TIMES];
uint8_t debug_wire_idx = 0;
static volatile uint8_t intdata_idx;
#define NUMBER_OF_INTDATA NUMBER_OF_DEBUG_WIRE_TIMES
static volatile uint32_t intdata_time[NUMBER_OF_INTDATA];
static volatile uint32_t intdata_intr[NUMBER_OF_INTDATA];
static volatile uint32_t intdata_intf[NUMBER_OF_INTDATA];

static bool parseintdata(void);
static void sampleoutputs(void);
static void trigger_wire_sensor(void);
static void togglepolarity(void);

void init_hal_sensors(void) {
    // Input pins seems to work ok, set outputs for controlling wire sensors
    //LPC_GPIO0->FIODIR |= (1<<21 | 1<<22);
    LPC_GPIOx(WIRESENS_RANGE_PORTNO)->FIODIR |= ( 1 << WIRESENS_RANGE_PINNO );
    LPC_GPIOx(WIRESENS_POLARITY_PORTNO)->FIODIR |= ( 1 << WIRESENS_POLARITY_PINNO );

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
    near_wire = false;
    debugmode = false;
    wirestate = wirestate_start_sampling;
}

void task_sensors(void)
{
    static systimer_t find_signal_timer, try_near_timer;
    static uint8_t no_of_failed_signaldet;
    if(!debugmode) {
        switch(wirestate) {
            case wirestate_start_sampling:
                near_wire = false;
                valid_signal_detected = false;
                LPC_GPIOx(WIRESENS_RANGE_PORTNO)->FIOSET = ( 1 << WIRESENS_RANGE_PINNO );
                trigger_wire_sensor();
                systimer_start(&find_signal_timer, FIND_SIGNAL_TIMEOUT_MS);
                wirestate = wirestate_near_wire;
                no_of_failed_signaldet = 0;
                break;
            case wirestate_near_wire:
                if(parseintdata()) {
                    no_of_failed_signaldet=0;
                    valid_signal_detected = true;
                    near_wire = true;
                    trigger_wire_sensor();
                    systimer_start(&find_signal_timer, FIND_SIGNAL_TIMEOUT_MS);
                    sampleoutputs();
                } else if(systimer_is_expired(&find_signal_timer) || intdata_idx==NUMBER_OF_INTDATA) {
                    valid_signal_detected = false;
                    if(no_of_failed_signaldet > FAILED_ATTEMPTS_BEFORE_TOGGLE_POLARITY) {
                        togglepolarity();
                    }
                    if(no_of_failed_signaldet++ > MAX_FAILED_ATTEMPTS_NEARWIRE) {
                        LPC_GPIOx(WIRESENS_RANGE_PORTNO)->FIOCLR = ( 1 << WIRESENS_RANGE_PINNO );
                        wirestate = wirestate_far_from_wire;
                        no_of_failed_signaldet=0;
                    }
                    trigger_wire_sensor();
                    systimer_start(&find_signal_timer, FIND_SIGNAL_TIMEOUT_MS);
                    systimer_start(&try_near_timer, TRY_NEAR_INTERVAL_MS);
                }
                break;
            case wirestate_far_from_wire:
                near_wire = false;
                if(parseintdata()) {
                    no_of_failed_signaldet=0;
                    valid_signal_detected = true;
                    if(systimer_is_expired(&try_near_timer)) {
                        LPC_GPIOx(WIRESENS_RANGE_PORTNO)->FIOSET = ( 1 << WIRESENS_RANGE_PINNO );
                        wirestate = wirestate_near_wire;
                        no_of_failed_signaldet = 0;
                    }
                    trigger_wire_sensor();
                    systimer_start(&find_signal_timer, FIND_SIGNAL_TIMEOUT_MS);
                    sampleoutputs();
                } else if(systimer_is_expired(&find_signal_timer) || intdata_idx==NUMBER_OF_INTDATA) {
                    if(no_of_failed_signaldet > FAILED_ATTEMPTS_BEFORE_TOGGLE_POLARITY) {
                        togglepolarity();
                    }
                    if(no_of_failed_signaldet++ > MAX_FAILED_ATTEMPTS_FARWIRE) {
                        left_wire_sensor = false;
                        right_wire_sensor = false;
                    }
                    valid_signal_detected = false;
                    trigger_wire_sensor();
                    systimer_start(&find_signal_timer, FIND_SIGNAL_TIMEOUT_MS);
                    sampleoutputs();
                }
                break;
        }
    } else {
        if(intdata_idx == NUMBER_OF_INTDATA) {
            for(debug_wire_idx=0 ; debug_wire_idx < NUMBER_OF_DEBUG_WIRE_TIMES ; debug_wire_idx++) {
                debug_wire_times[debug_wire_idx] = intdata_time[debug_wire_idx];
                debug_wire_values[debug_wire_idx] = ((intdata_intr[debug_wire_idx] >> 3) & 0x50) | ((intdata_intf[debug_wire_idx] >> 8) & 0x05);
            }
            intdata_idx = 0;
        }
    }
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
    } else if(sensor == SENSOR_NEAR_WIRE) {
        return near_wire;
    }

    return false; // Should never happen, todo: assert
}

int get_wiredistance(void)
{
    return time_between_firstedges;
}

#define FILTER_MAX_COUNT 6
#define FILTER_LIMIT (FILTER_MAX_COUNT/2)
static uint8_t updatesignal(bool signalvalue, uint8_t oldcount)
{
    if(signalvalue) {
        if(oldcount < FILTER_MAX_COUNT) {
            return(oldcount + 1);
        } else {
            return FILTER_MAX_COUNT;
        }
    } else {
        if(oldcount > 0) {
            return(oldcount - 1);
        }
    }
    return 0;
}

static void sampleoutputs(void)
{
    static uint8_t near_wire_count=0, left_count=0, right_count=0;
    
    near_wire_count = updatesignal(near_wire, near_wire_count);
    filtered_near_wire = (near_wire_count >= FILTER_LIMIT);

    left_count = updatesignal(left_wire_sensor, left_count);
    filtered_left_wire_sensor = (left_count >= FILTER_LIMIT);

    right_count = updatesignal(right_wire_sensor, right_count);
    filtered_right_wire_sensor = (right_count >= FILTER_LIMIT);
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
  Triggers a new read of the wire sensors. Starts writing from beginning of intdata buffer and enables interrupt.
  Interrupt is disabled by isr when sample buffer is filled.
*/
static void trigger_wire_sensor(void)
{
    NVIC_DisableIRQ(EINT3_IRQn);

    // Reset timer0
    LPC_TIM0->TCR = 0x02;
    LPC_TIM0->TCR = 0x01;
    // Enable interrupts on wire signals
    LPC_GPIOINT->IO0IntEnR = (RIGHT_SENSOR_RISING_EDGE_BITVAL | LEFT_SENSOR_RISING_EDGE_BITVAL);
    LPC_GPIOINT->IO0IntEnF = (RIGHT_SENSOR_FALLING_EDGE_BITVAL | LEFT_SENSOR_FALLING_EDGE_BITVAL);

    intdata_idx = 0;

    NVIC_EnableIRQ(EINT3_IRQn);
}

/* Conclusions of test with wire sensors
     If the first edge after a long time without edges (at least 0x40000 and not more than 0x50000 TIMER0-ticks) is a rising edge that sensor is inside the wire
     If the first edge after a long time without edges (at least 0x40000 and not more than 0x50000 TIMER0-ticks) is a falling edge that sensor is outside the wire
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

    if(intdata_idx < NUMBER_OF_INTDATA) {
        intdata_time[intdata_idx] = time;
        intdata_intr[intdata_idx] = intr;
        intdata_intf[intdata_idx] = intf;
        intdata_idx++;
    } 
    if(intdata_idx >= NUMBER_OF_INTDATA) {
        NVIC_DisableIRQ(EINT3_IRQn);
        LPC_GPIOINT->IO0IntEnR = 0;
        LPC_GPIOINT->IO0IntEnF = 0;
    }
}

void wire_sensor_debug(bool debug_enable, bool polarity, bool near_range, bool restart_samples) {
    if(debug_enable) {
        debugmode = true;
    } else {
        debugmode = false;
    }

    if(near_range) {
        LPC_GPIOx(WIRESENS_RANGE_PORTNO)->FIOSET = ( 1 << WIRESENS_RANGE_PINNO );
    } else {
        LPC_GPIOx(WIRESENS_RANGE_PORTNO)->FIOCLR = ( 1 << WIRESENS_RANGE_PINNO );
    }

    if(polarity) {
        LPC_GPIOx(WIRESENS_POLARITY_PORTNO)->FIOSET = ( 1 << WIRESENS_POLARITY_PINNO );
    } else {
        LPC_GPIOx(WIRESENS_POLARITY_PORTNO)->FIOCLR = ( 1 << WIRESENS_POLARITY_PINNO );
    }

    if(restart_samples) {
        intdata_idx = 0;
        trigger_wire_sensor();
    }
}

static bool both_rising_and_falling_edges(uint8_t sampleidx)
{
    if((intdata_intf[sampleidx] & LEFT_SENSOR_FALLING_EDGE_BITVAL) > 0 && (intdata_intr[sampleidx] & LEFT_SENSOR_RISING_EDGE_BITVAL) > 0) {
        return true;
    }
    if((intdata_intf[sampleidx] & RIGHT_SENSOR_FALLING_EDGE_BITVAL) > 0 && (intdata_intr[sampleidx] & RIGHT_SENSOR_RISING_EDGE_BITVAL) > 0) {
        return true;
    }
    return false;
}

static bool parseintdata_fromstartpulse(uint8_t startpulse_idx)
{
    int local_timediff = 0;
    uint8_t sampleidx = startpulse_idx;
    bool left_firstedge_detected = false;
    bool right_firstedge_detected = false;
    bool left_secondedge_detected = false;
    bool right_secondedge_detected = false;
    bool left_firstedge_rising = false;
    bool right_firstedge_rising = false;

    if(both_rising_and_falling_edges(sampleidx)) {
        return false;
    }
    if((intdata_intf[sampleidx] & LEFT_SENSOR_FALLING_EDGE_BITVAL) > 0) {
        left_firstedge_detected = true;
        left_firstedge_rising = false;
    } else if((intdata_intr[sampleidx] & LEFT_SENSOR_RISING_EDGE_BITVAL) > 0) {
        left_firstedge_detected = true;
        left_firstedge_rising = true;
    }
    if((intdata_intf[sampleidx] & RIGHT_SENSOR_FALLING_EDGE_BITVAL) > 0) {
        right_firstedge_detected = true;
        right_firstedge_rising = false;
    } else if((intdata_intr[sampleidx] & RIGHT_SENSOR_RISING_EDGE_BITVAL) > 0) {
        right_firstedge_detected = true;
        right_firstedge_rising = true;
    }
    if(++sampleidx >= intdata_idx) {
        return false;
    }
    // First edge for left, right or both channels have been found. If only one then the next should be in this sample
    if(!left_firstedge_detected) {
        if(intdata_time[sampleidx] > MAX_TIME_BETWEEN_FIRSTEDGES) {
            return false;
        }
        local_timediff = -intdata_time[sampleidx];
        if((intdata_intf[sampleidx] & LEFT_SENSOR_FALLING_EDGE_BITVAL) > 0) {
            left_firstedge_detected = true;
            left_firstedge_rising = false;
        } else if((intdata_intr[sampleidx] & LEFT_SENSOR_RISING_EDGE_BITVAL) > 0) {
            left_firstedge_detected = true;
            left_firstedge_rising = true;
        }
        if(++sampleidx >= intdata_idx) {
           return false;
        }
    }
    else if(!right_firstedge_detected) {
        if(intdata_time[sampleidx] > MAX_TIME_BETWEEN_FIRSTEDGES) {
            return false;
        }
        local_timediff = intdata_time[sampleidx];
        if((intdata_intf[sampleidx] & RIGHT_SENSOR_FALLING_EDGE_BITVAL) > 0) {
            right_firstedge_detected = true;
            right_firstedge_rising = false;
        } else if((intdata_intr[sampleidx] & RIGHT_SENSOR_RISING_EDGE_BITVAL) > 0) {
            right_firstedge_detected = true;
            right_firstedge_rising = true;
        }
        if(++sampleidx >= intdata_idx) {
           return false;
        }        
    }

    // Both first edges are now found. Next one or two samples should contain the other edge (rising/falling)
    if(intdata_time[sampleidx] > MAX_TIME_BEFORE_OTHEREDGE) {
        return false;
    }
    if(left_firstedge_rising) {
        if((intdata_intf[sampleidx] & LEFT_SENSOR_FALLING_EDGE_BITVAL) > 0) {
            left_secondedge_detected = true;
        }
    } else {
        if((intdata_intr[sampleidx] & LEFT_SENSOR_RISING_EDGE_BITVAL) > 0) {
            left_secondedge_detected = true;
        }
    }
    if(right_firstedge_rising) {
        if((intdata_intf[sampleidx] & RIGHT_SENSOR_FALLING_EDGE_BITVAL) > 0) {
            right_secondedge_detected = true;
        }
    } else {
        if((intdata_intr[sampleidx] & RIGHT_SENSOR_RISING_EDGE_BITVAL) > 0) {
            right_secondedge_detected = true;
        }
    }

    // Check if we are done or if we need to check for another edge
    if(!left_secondedge_detected || !right_secondedge_detected) {
        if(++sampleidx >= intdata_idx) {
           return false;
        }        
        if(intdata_time[sampleidx] > MAX_TIME_BEFORE_OTHEREDGE) {
            return false;
        }
        if(left_firstedge_rising) {
            if((intdata_intf[sampleidx] & LEFT_SENSOR_FALLING_EDGE_BITVAL) > 0) {
                left_secondedge_detected = true;
            }
        } else {
            if((intdata_intr[sampleidx] & LEFT_SENSOR_RISING_EDGE_BITVAL) > 0) {
                left_secondedge_detected = true;
            }
        }
        if(right_firstedge_rising) {
            if((intdata_intf[sampleidx] & RIGHT_SENSOR_FALLING_EDGE_BITVAL) > 0) {
                right_secondedge_detected = true;
            }
        } else {
            if((intdata_intr[sampleidx] & RIGHT_SENSOR_RISING_EDGE_BITVAL) > 0) {
                right_secondedge_detected = true;
            }
        }
    }

    if(!left_secondedge_detected || !right_secondedge_detected) {
        return false;
    }    

    // Both rising and falling edges have been found for both channels within all time limits. Update result variables
    left_wire_sensor = left_firstedge_rising;
    right_wire_sensor = right_firstedge_rising;
    time_between_firstedges = local_timediff;
    return true;
}

// parses intdata and updates right_wire_sensor and left_wire_sensor if valid data is found. No update is made if valid data is not found.
// returns true if valid pulses are found in intdata
static bool parseintdata(void)
{
    uint8_t sampleidx;

    // search for a pulse after a long time with no pulses
    for(sampleidx=1 ; sampleidx < intdata_idx ; sampleidx++) { //skip first data item, it is invalid
        if((intdata_time[sampleidx] > LONG_WIRESENSOR_IDLE_TIME && intdata_time[sampleidx] < TOO_LONG_WIRESENSOR_IDLE_TIME)) {
            if(parseintdata_fromstartpulse(sampleidx)) {
                return true;
            }
        }
    }
    return false;
}

static void togglepolarity(void) {
    static bool polarity = false;
    polarity = !polarity;
    if(polarity) {
        LPC_GPIOx(WIRESENS_POLARITY_PORTNO)->FIOSET = ( 1 << WIRESENS_POLARITY_PINNO );
    } else {
        LPC_GPIOx(WIRESENS_POLARITY_PORTNO)->FIOCLR = ( 1 << WIRESENS_POLARITY_PINNO );
    }
}
