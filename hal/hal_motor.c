#include "LPC17xx.h"
#include "hal_motor.h"
#include <stdlib.h>
#include <stdbool.h>

#define RIGHT_PWM_PORTNO         2
#define RIGHT_PWM_PINNO          0
#define LEFT_PWM_PORTNO          2
#define LEFT_PWM_PINNO           1
#define SPINDLE_PWM_PORTNO       2
#define SPINDLE_PWM_PINNO        2

#define RIGHT_ENABLE_PORTNO      2
#define RIGHT_ENABLE_PINNO       4
#define LEFT_ENABLE_PORTNO       2
#define LEFT_ENABLE_PINNO        9
#define SPINDLE_ENABLE_PORTNO    2
#define SPINDLE_ENABLE_PINNO     13

#define RIGHT_BRAKE_PORTNO       2
#define RIGHT_BRAKE_PINNO        5
#define LEFT_BRAKE_PORTNO        2
#define LEFT_BRAKE_PINNO         8
#define SPINDLE_BRAKE_PORTNO     3
#define SPINDLE_BRAKE_PINNO      25

#define RIGHT_DIRECTION_PORTNO   2 // 1=FORWARD, 0=BACKWARDS
#define RIGHT_DIRECTION_PINNO    6
#define LEFT_DIRECTION_PORTNO    0
#define LEFT_DIRECTION_PINNO     0
#define SPINDLE_DIRECTION_PORTNO 3
#define SPINDLE_DIRECTION_PINNO  26

static uint8_t ramps[MOTOR_NUMBER_OF_MOTORS] = {100, 100, 100};
static int8_t requestedspeed[MOTOR_NUMBER_OF_MOTORS] = {0};
static int32_t currentspeed_times_100[MOTOR_NUMBER_OF_MOTORS] = {0};

void init_hal_motor(void) {
  //LPC_GPIO1->FIODIR |= ( 1 << POWERON_PINNO );
  //LPC_GPIO1->FIOSET = ( 1 << POWERON_PINNO );
}

static void setdirection(motors_t motor, uint8_t value)
{
  // todo: implement this
}

static void setpwm(motors_t motor, uint32_t value_times_100)
{
  // todo: implement this
}

void motor_task(void)
{
  for(uint8_t i=0 ; i < MOTOR_NUMBER_OF_MOTORS ; i++) {
    int32_t speeddiff = ((int32_t)requestedspeed[i] * 100) - currentspeed_times_100[i];
    speeddiff = speeddiff * ramps[i] / 100;
    currentspeed_times_100[i] += speeddiff;

    if(currentspeed_times_100[i] > 0) {
      setdirection(i, 1);
    } else {
      setdirection(i, 0);
    }

    setpwm(i, labs(currentspeed_times_100[i]));
  }
}

/* Controls the rate motor speed changes when changing motor speed. 
   1=Speed changes 1% per 100ms? 
   100=Speed changes immediately */
void set_motor_ramp(motors_t motor, uint8_t ramp_percent)
{
  if(ramp_percent < 1) {
    ramp_percent = 1;
  }
  if(ramp_percent > 100) {
    ramp_percent = 100;
  }
  ramps[motor] = ramp_percent;
}

/* Controls the motor speed.
   100 = max speed forward, 0 = motor stopped, -100 = max speed backwards
*/
void set_motor_speed(motors_t motor, int8_t speed)
{
  requestedspeed[motor] = speed;
  motor_task(); // make sure currentspeed is updated immediately (if ramp is 100%)
}
