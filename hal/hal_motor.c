#include "hal_mcu.h"
#include "hal_motor.h"
#include "system.h"
#include <stdlib.h>
#include <stdbool.h>

#define RIGHT_PWM_PORTNO         2
#define RIGHT_PWM_PINNO          0
#define LEFT_PWM_PORTNO          2
#define LEFT_PWM_PINNO           1
#define SPINDLE_PWM_PORTNO       2
#define SPINDLE_PWM_PINNO        2

#define RIGHT_ENABLE_PORTNO      2 // Needs to be 1 for motor to run. 0=motor freewheel
#define RIGHT_ENABLE_PINNO       4
#define LEFT_ENABLE_PORTNO       2
#define LEFT_ENABLE_PINNO        9
#define SPINDLE_ENABLE_PORTNO    2
#define SPINDLE_ENABLE_PINNO     13

#define RIGHT_BRAKE_PORTNO       2 // Needs to be 0 for motor to run
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

#define PWM_COUNTER_MAXVALUE 1000 // 2kHz

static uint8_t ramps[MOTOR_NUMBER_OF_MOTORS] = {100, 100, 100};
static int8_t requestedspeed[MOTOR_NUMBER_OF_MOTORS] = {0};
static int32_t currentspeed_times_100[MOTOR_NUMBER_OF_MOTORS] = {0};

void init_hal_motor(void) {
  LPC_SC->PCONP |= (1 << 6);   // power up PWM1

  // Note that these values must be updated if x_PWM_PINNO or PORTNO changes
  LPC_PINCON->PINSEL4 &= ~0x3f; // Reset all bits of port2.0-port2.2
  LPC_PINCON->PINSEL4 |= 0x15;  // Set port2.0-port2.2 to alternate function 01 (PWM)

  LPC_PWM1->PCR &= ~(0x0e00); // Disable pwm 1-3
  LPC_PWM1->PR = 12; // The TC is incremented every PR+1 cycles of PCLK.
  LPC_PWM1->MR0 = PWM_COUNTER_MAXVALUE; // 2khz
  LPC_PWM1->MR1 = 0; // PWM1
  LPC_PWM1->MR2 = 0; // PWM2
  LPC_PWM1->MR3 = 0; // PWM3
  LPC_PWM1->MCR = 0x02; // Reset Timer1 when MR0 matches timer counter
  LPC_PWM1->LER = 0x0f; // Update MR0-MR3 on next timer1 reset. Note that this write disables update of other MR-registers if the bits were set
  LPC_PWM1->PCR |= (0x0e00); // Enable pwm 1-3

  //LPC_PWM1->TCR = BIT(1);             // Counter Reset

  LPC_PWM1->TCR = 0x09;    // PWM Timer Counter enable and PWM Enable

  LPC_GPIOx(RIGHT_DIRECTION_PORTNO)->FIODIR |= ( 1 << RIGHT_DIRECTION_PINNO);
  LPC_GPIOx(RIGHT_DIRECTION_PORTNO)->FIOSET = ( 1 << RIGHT_DIRECTION_PINNO);
  LPC_GPIOx(LEFT_DIRECTION_PORTNO)->FIODIR |= ( 1 << LEFT_DIRECTION_PINNO);
  LPC_GPIOx(LEFT_DIRECTION_PORTNO)->FIOSET = ( 1 << LEFT_DIRECTION_PINNO);
  LPC_GPIOx(SPINDLE_DIRECTION_PORTNO)->FIODIR |= ( 1 << SPINDLE_DIRECTION_PINNO);
  LPC_GPIOx(SPINDLE_DIRECTION_PORTNO)->FIOSET = ( 1 << SPINDLE_DIRECTION_PINNO);

  LPC_GPIOx(RIGHT_BRAKE_PORTNO)->FIODIR |= ( 1 << RIGHT_BRAKE_PINNO);
  LPC_GPIOx(RIGHT_BRAKE_PORTNO)->FIOCLR = ( 1 << RIGHT_BRAKE_PINNO);
  LPC_GPIOx(LEFT_BRAKE_PORTNO)->FIODIR |= ( 1 << LEFT_BRAKE_PINNO);
  LPC_GPIOx(LEFT_BRAKE_PORTNO)->FIOCLR = ( 1 << LEFT_BRAKE_PINNO);
  LPC_GPIOx(SPINDLE_BRAKE_PORTNO)->FIODIR |= ( 1 << SPINDLE_BRAKE_PINNO);
  LPC_GPIOx(SPINDLE_BRAKE_PORTNO)->FIOCLR = ( 1 << SPINDLE_BRAKE_PINNO);

  LPC_GPIOx(RIGHT_ENABLE_PORTNO)->FIODIR |= ( 1 << RIGHT_ENABLE_PINNO);
  LPC_GPIOx(RIGHT_ENABLE_PORTNO)->FIOSET = ( 1 << RIGHT_ENABLE_PINNO);
  LPC_GPIOx(LEFT_ENABLE_PORTNO)->FIODIR |= ( 1 << LEFT_ENABLE_PINNO);
  LPC_GPIOx(LEFT_ENABLE_PORTNO)->FIOSET = ( 1 << LEFT_ENABLE_PINNO);
  LPC_GPIOx(SPINDLE_ENABLE_PORTNO)->FIODIR |= ( 1 << SPINDLE_ENABLE_PINNO);
  LPC_GPIOx(SPINDLE_ENABLE_PORTNO)->FIOSET = ( 1 << SPINDLE_ENABLE_PINNO);

/*
// TMP DEBUG
  LPC_GPIOx(LEFT_DIRECTION_PORTNO)->FIODIR |= ( 1 << LEFT_DIRECTION_PINNO);
  LPC_GPIOx(LEFT_DIRECTION_PORTNO)->FIOSET = ( 1 << LEFT_DIRECTION_PINNO);
  LPC_GPIOx(LEFT_BRAKE_PORTNO)->FIODIR |= ( 1 << LEFT_BRAKE_PINNO);
  LPC_GPIOx(LEFT_BRAKE_PORTNO)->FIOCLR = ( 1 << LEFT_BRAKE_PINNO);
  LPC_GPIOx(LEFT_ENABLE_PORTNO)->FIODIR |= ( 1 << LEFT_ENABLE_PINNO);
  LPC_GPIOx(LEFT_ENABLE_PORTNO)->FIOCLR = ( 1 << LEFT_ENABLE_PINNO);
  LPC_GPIOx(LEFT_PWM_PORTNO)->FIODIR |= ( 1 << LEFT_PWM_PINNO);
  LPC_GPIOx(LEFT_PWM_PORTNO)->FIOCLR = ( 1 << LEFT_PWM_PINNO);
  */
}

static void setdirection(motors_t motor, uint8_t value)
{
  switch(motor) {
    case MOTOR_RIGHT:
      if(value) {
        LPC_GPIOx(RIGHT_DIRECTION_PORTNO)->FIOSET = ( 1 << RIGHT_DIRECTION_PINNO);
      } else {
        LPC_GPIOx(RIGHT_DIRECTION_PORTNO)->FIOCLR = ( 1 << RIGHT_DIRECTION_PINNO);
      }
      break;
    case MOTOR_LEFT:
      if(value) {
        LPC_GPIOx(LEFT_DIRECTION_PORTNO)->FIOSET = ( 1 << LEFT_DIRECTION_PINNO);
      } else {
        LPC_GPIOx(LEFT_DIRECTION_PORTNO)->FIOCLR = ( 1 << LEFT_DIRECTION_PINNO);
      }
      break;
    case MOTOR_SPINDLE:
      if(value) {
        LPC_GPIOx(SPINDLE_DIRECTION_PORTNO)->FIOSET = ( 1 << SPINDLE_DIRECTION_PINNO);
      } else {
        LPC_GPIOx(SPINDLE_DIRECTION_PORTNO)->FIOCLR = ( 1 << SPINDLE_DIRECTION_PINNO);
      }
      break;
    default:
      DOASSERT();
  }
}

static void setpwm(motors_t motor, uint32_t value_times_100)
{
  uint32_t timervalue;
  timervalue = value_times_100 * PWM_COUNTER_MAXVALUE / 10000;
  switch(motor) {
    case MOTOR_RIGHT:
      LPC_PWM1->MR1 = timervalue;
      break;
    case MOTOR_LEFT:
      LPC_PWM1->MR2 = timervalue;
      break;
    case MOTOR_SPINDLE:
      LPC_PWM1->MR3 = timervalue;
      break;
    default:
      DOASSERT();
  }
  LPC_PWM1->LER = 0x0f; // Update MR0-MR3 on next timer1 reset. Note that this write disables update of other MR-registers if the bits were set

}

// This function shall be called every 100ms for ramp speeds to work
static void update_motor_speed(void)
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

void task_motor(void)
{
  static uint32_t lastupdatetick=0;

  if(((systick_cnt-lastupdatetick) / SYS_TICK_PERIOD_IN_MS) > 100) {
    update_motor_speed();
    lastupdatetick = systick_cnt;
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
  update_motor_speed(); // make sure currentspeed is updated immediately (if ramp is 100%)
}
