#include "hal_mcu.h"
#include "hal_motor.h"
#include "system.h"
#include <stdlib.h>
#include <stdbool.h>

#define RIGHT_PWM_PORTNO         2 // 1 moves motor, 0 stops motor
#define RIGHT_PWM_PINNO          0
#define LEFT_PWM_PORTNO          2
#define LEFT_PWM_PINNO           1
#define SPINDLE_PWM_PORTNO       2
#define SPINDLE_PWM_PINNO        2

#define RIGHT_ENABLE_PORTNO      2 // Needs to be 0 for motor to run and to brake.
#define RIGHT_ENABLE_PINNO       4
#define LEFT_ENABLE_PORTNO       2 // Inverted?: Needs to be 1 for motor to run and to brake.
#define LEFT_ENABLE_PINNO        9
#define SPINDLE_ENABLE_PORTNO    2 // Needs to be 0 for motor to run and to brake.
#define SPINDLE_ENABLE_PINNO     13

#define RIGHT_BRAKE_PORTNO       2 // Needs to be 1 for motor to run. 0 brakes motor.
#define RIGHT_BRAKE_PINNO        5
#define LEFT_BRAKE_PORTNO        2 // Inverted? Needs to be 0 for motor to run. 1 brakes motor.
#define LEFT_BRAKE_PINNO         8
#define SPINDLE_BRAKE_PORTNO     3 // Needs to be 1 for motor to run. 0 brakes motor.
#define SPINDLE_BRAKE_PINNO      25

#define BRAKE_MOTOR(x) do { if((x) == 0) LPC_GPIOx(RIGHT_BRAKE_PORTNO)->FIOCLR = (1u << RIGHT_BRAKE_PINNO); else LPC_GPIOx(LEFT_BRAKE_PORTNO)->FIOSET = (1u << LEFT_BRAKE_PINNO); } while(0)
#define RELEASE_BRAKE(x) do { if((x) == 0) LPC_GPIOx(RIGHT_BRAKE_PORTNO)->FIOSET = (1u << RIGHT_BRAKE_PINNO); else LPC_GPIOx(LEFT_BRAKE_PORTNO)->FIOCLR = (1u << LEFT_BRAKE_PINNO); } while(0)

#define RIGHT_DIRECTION_PORTNO   2 // 1=FORWARD, 0=BACKWARDS
#define RIGHT_DIRECTION_PINNO    6 
#define LEFT_DIRECTION_PORTNO    0
#define LEFT_DIRECTION_PINNO     0 // Inverted?  0=FORWARD, 1=BACKWARDS
#define SPINDLE_DIRECTION_PORTNO 3
#define SPINDLE_DIRECTION_PINNO  26

#define RIGHT_PULSE_COUNT_PORTNO  2
#define RIGHT_PULSE_COUNT_PINNO   11
#define LEFT_PULSE_COUNT_PORTNO   2
#define LEFT_PULSE_COUNT_PINNO    12

#define PWM_COUNTER_MAXVALUE 1000 // 2kHz
#define MOTOR_IDLE_DISABLE_TIME_MS 10000

static uint8_t ramps[MOTOR_NUMBER_OF_MOTORS] = {100, 100, 100};
static int8_t requestedspeed[MOTOR_NUMBER_OF_MOTORS] = {0};
static int32_t currentspeed_times_100[MOTOR_NUMBER_OF_MOTORS] = {0};
static volatile bool motor_direction_sign[MOTOR_NUMBER_OF_MOTORS] = {false, false, false};
static volatile int32_t motor_pulse_count[MOTOR_NUMBER_OF_MOTORS] __attribute__((aligned(4))) = {0};
static int32_t motor_prev_pulse_count[MOTOR_NUMBER_OF_MOTORS] = {0};
static int32_t motor_speed_steps[MOTOR_NUMBER_OF_MOTORS] = {0};
static systimer_t motor_speed_timer;
static uint16_t motor_idle_ticks = 0;
static bool motors_enabled = false;

static void set_all_motors_enabled(bool enabled)
{
  if(enabled) {
    LPC_GPIOx(RIGHT_ENABLE_PORTNO)->FIOCLR = (1u << RIGHT_ENABLE_PINNO);
    LPC_GPIOx(LEFT_ENABLE_PORTNO)->FIOSET = (1u << LEFT_ENABLE_PINNO);
    LPC_GPIOx(SPINDLE_ENABLE_PORTNO)->FIOCLR = (1u << SPINDLE_ENABLE_PINNO);
  } else {
    LPC_GPIOx(RIGHT_ENABLE_PORTNO)->FIOSET = (1u << RIGHT_ENABLE_PINNO);
    LPC_GPIOx(LEFT_ENABLE_PORTNO)->FIOCLR = (1u << LEFT_ENABLE_PINNO);
    LPC_GPIOx(SPINDLE_ENABLE_PORTNO)->FIOSET = (1u << SPINDLE_ENABLE_PINNO);
  }
  motors_enabled = enabled;
}

void __attribute__ ((interrupt)) EINT1_IRQHandler(void)
{
  LPC_SC->EXTINT = (1u << 1);
  if(motor_direction_sign[MOTOR_RIGHT]) {
    motor_pulse_count[MOTOR_RIGHT]++;
  } else {
    motor_pulse_count[MOTOR_RIGHT]--;
  }
}

void __attribute__ ((interrupt)) EINT2_IRQHandler(void)
{
  LPC_SC->EXTINT = (1u << 2);
  if(motor_direction_sign[MOTOR_LEFT]) {
    motor_pulse_count[MOTOR_LEFT]++;
  } else {
    motor_pulse_count[MOTOR_LEFT]--;
  }
}

void init_hal_motor(void) {
  //Start with motors disabled until pwms are initialized
  LPC_GPIOx(RIGHT_ENABLE_PORTNO)->FIODIR |= ( 1 << RIGHT_ENABLE_PINNO);
  LPC_GPIOx(RIGHT_ENABLE_PORTNO)->FIOSET = ( 1 << RIGHT_ENABLE_PINNO);
  LPC_GPIOx(LEFT_ENABLE_PORTNO)->FIODIR |= ( 1 << LEFT_ENABLE_PINNO);
  LPC_GPIOx(LEFT_ENABLE_PORTNO)->FIOCLR = ( 1 << LEFT_ENABLE_PINNO);
  LPC_GPIOx(SPINDLE_ENABLE_PORTNO)->FIODIR |= ( 1 << SPINDLE_ENABLE_PINNO);
  LPC_GPIOx(SPINDLE_ENABLE_PORTNO)->FIOSET = ( 1 << SPINDLE_ENABLE_PINNO);

  LPC_GPIOx(RIGHT_DIRECTION_PORTNO)->FIODIR |= ( 1 << RIGHT_DIRECTION_PINNO);
  LPC_GPIOx(RIGHT_DIRECTION_PORTNO)->FIOSET = ( 1 << RIGHT_DIRECTION_PINNO);
  LPC_GPIOx(LEFT_DIRECTION_PORTNO)->FIODIR |= ( 1 << LEFT_DIRECTION_PINNO);
  LPC_GPIOx(LEFT_DIRECTION_PORTNO)->FIOSET = ( 1 << LEFT_DIRECTION_PINNO);
  LPC_GPIOx(SPINDLE_DIRECTION_PORTNO)->FIODIR |= ( 1 << SPINDLE_DIRECTION_PINNO);
  LPC_GPIOx(SPINDLE_DIRECTION_PORTNO)->FIOSET = ( 1 << SPINDLE_DIRECTION_PINNO);

  LPC_GPIOx(RIGHT_BRAKE_PORTNO)->FIODIR |= ( 1 << RIGHT_BRAKE_PINNO);
  LPC_GPIOx(RIGHT_BRAKE_PORTNO)->FIOSET = ( 1 << RIGHT_BRAKE_PINNO);
  LPC_GPIOx(LEFT_BRAKE_PORTNO)->FIODIR |= ( 1 << LEFT_BRAKE_PINNO);
  LPC_GPIOx(LEFT_BRAKE_PORTNO)->FIOCLR = ( 1 << LEFT_BRAKE_PINNO);
  LPC_GPIOx(SPINDLE_BRAKE_PORTNO)->FIODIR |= ( 1 << SPINDLE_BRAKE_PINNO);
  LPC_GPIOx(SPINDLE_BRAKE_PORTNO)->FIOSET = ( 1 << SPINDLE_BRAKE_PINNO);
  
  LPC_GPIOx(RIGHT_PWM_PORTNO)->FIODIR |= ( 1 << RIGHT_PWM_PINNO);
  LPC_GPIOx(RIGHT_PWM_PORTNO)->FIOCLR = ( 1 << RIGHT_PWM_PINNO);
  LPC_GPIOx(LEFT_PWM_PORTNO)->FIODIR |= ( 1 << LEFT_PWM_PINNO);
  LPC_GPIOx(LEFT_PWM_PORTNO)->FIOCLR = ( 1 << LEFT_PWM_PINNO);
  LPC_GPIOx(SPINDLE_PWM_PORTNO)->FIODIR |= ( 1 << SPINDLE_PWM_PINNO);
  LPC_GPIOx(SPINDLE_PWM_PORTNO)->FIOCLR = ( 1 << SPINDLE_PWM_PINNO);

  LPC_GPIOx(RIGHT_PULSE_COUNT_PORTNO)->FIODIR &= ~(1 << RIGHT_PULSE_COUNT_PINNO);
  LPC_GPIOx(LEFT_PULSE_COUNT_PORTNO)->FIODIR &= ~(1 << LEFT_PULSE_COUNT_PINNO);

  LPC_PINCON->PINSEL4 &= ~((3u << (RIGHT_PULSE_COUNT_PINNO * 2)) | (3u << (LEFT_PULSE_COUNT_PINNO * 2)));
  LPC_PINCON->PINSEL4 |= (1u << (RIGHT_PULSE_COUNT_PINNO * 2)) | (1u << (LEFT_PULSE_COUNT_PINNO * 2));

  LPC_SC->EXTMODE |= (1u << 1) | (1u << 2);
  LPC_SC->EXTPOLAR |= (1u << 1) | (1u << 2);
  LPC_SC->EXTINT = (1u << 1) | (1u << 2);
  NVIC_EnableIRQ(EINT1_IRQn);
  NVIC_EnableIRQ(EINT2_IRQn);

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
  LPC_PWM1->LER |= 0x0f; // Update MR0-MR3 on next timer1 reset.

  LPC_PWM1->TCR = 0x02;    // Reset counter
  LPC_PWM1->TCR = 0x01;    // Release reset, enable counter

  LPC_PWM1->PCR |= (0x0e00); // Enable pwm 1-3

  // Wait until all pwms are low. Why is this needed???
  for(long a=0;a<10000000UL;a++)
  {
    asm("nop");
  }

  // Enable motors now that pwm is enabled.
  set_all_motors_enabled(true);

  motor_direction_sign[MOTOR_RIGHT] = false;
  motor_direction_sign[MOTOR_LEFT] = false;
  motor_direction_sign[MOTOR_SPINDLE] = false;
  motor_pulse_count[MOTOR_RIGHT] = 0;
  motor_pulse_count[MOTOR_LEFT] = 0;
  motor_pulse_count[MOTOR_SPINDLE] = 0;
  systimer_start(&motor_speed_timer, 100);
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
        LPC_GPIOx(LEFT_DIRECTION_PORTNO)->FIOCLR = ( 1 << LEFT_DIRECTION_PINNO);
      } else {
        LPC_GPIOx(LEFT_DIRECTION_PORTNO)->FIOSET = ( 1 << LEFT_DIRECTION_PINNO);
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
      LPC_PWM1->LER |= (1 << 1);
      break;
    case MOTOR_LEFT:
      LPC_PWM1->MR2 = timervalue;
      LPC_PWM1->LER |= (1 << 2);
      break;
    case MOTOR_SPINDLE:
      LPC_PWM1->MR3 = timervalue;
      LPC_PWM1->LER |= (1 << 3);
      break;
    default:
      DOASSERT();
  }

}

#define PULSES_PER_100MS_AT_100_PERCENT_SPEED 19
static void update_motor_speed(void)
{
  bool timer_expired = false;
  bool any_requested_speed = false;
  
  if(systimer_is_expired(&motor_speed_timer)) {
    timer_expired = true;
    systimer_start(&motor_speed_timer, 100);
  }

  // Disable motors if all motors are off for a while
  for(uint8_t i=0 ; i < MOTOR_NUMBER_OF_MOTORS ; i++) {
    if(requestedspeed[i] != 0) {
      any_requested_speed = true;
      break;
    }
  }
  if(any_requested_speed) {
    if(!motors_enabled) {
      set_all_motors_enabled(true);
    }
    motor_idle_ticks = 0;
  } else if(timer_expired && motor_idle_ticks < (MOTOR_IDLE_DISABLE_TIME_MS / 100)) {
    motor_idle_ticks++;
    if(motor_idle_ticks >= (MOTOR_IDLE_DISABLE_TIME_MS / 100) && motors_enabled) {
      set_all_motors_enabled(false);
    }
  }

  for(uint8_t i=0 ; i < MOTOR_NUMBER_OF_MOTORS ; i++) {
    // Measure motor speed
    if(timer_expired) {
      motor_speed_steps[i] = motor_pulse_count[i] - motor_prev_pulse_count[i];
      motor_prev_pulse_count[i] = motor_pulse_count[i];
    }
    // Brake wheel motors if going too fast forward (downhill)
    if(i != MOTOR_SPINDLE && currentspeed_times_100[i] >= 0) {
      if(motor_pulse_count[i] - motor_prev_pulse_count[i] > (PULSES_PER_100MS_AT_100_PERCENT_SPEED * (uint32_t)currentspeed_times_100 / 100)) {
        BRAKE_MOTOR(i);
      } else {
        RELEASE_BRAKE(i);
      }
    }
    // Control motor speed ramps
    if(timer_expired || ramps[i] == 100) {
      int32_t speeddiff = ((int32_t)requestedspeed[i] * 100) - currentspeed_times_100[i];
      speeddiff = speeddiff * ramps[i] / 100;
      currentspeed_times_100[i] += speeddiff;

      if(currentspeed_times_100[i] >= 0) {
        setdirection(i, 0);
        motor_direction_sign[i] = true;
      } else {
        setdirection(i, 1);
        motor_direction_sign[i] = false;
      }

      setpwm(i, labs(currentspeed_times_100[i]));
    }
  }
}

void task_motor(void)
{
  update_motor_speed();
}

int32_t get_motor_distance(motors_t motor)
{
  return motor_pulse_count[motor];
}

int32_t get_motor_speed(motors_t motor)
{
  return motor_speed_steps[motor];
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
