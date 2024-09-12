#include "system.h"
#include <stdio.h>
#include <string.h>
#include "hal/hal_display.h"
#include "hal/hal_keyboard.h"
#include "hal/hal_sensors.h"
#include "hal/hal_motor.h"

#include "hal/hal_mcu.h"//debug

static uint8_t rightspeed=0, leftspeed=0, spindlespeed=0;

#define FONT_HEIGHT 13
#define FONT_WIDTH 7

#define CHARS_PER_ROW 20
static char textStr[CHARS_PER_ROW] = "";

void init_display(void) {
    u8g_FirstPage(&u8g);
    do {
        u8g_SetDefaultBackgroundColor(&u8g);
        u8g_DrawBox(&u8g, 0, 0, 128, 64);
        u8g_SetDefaultForegroundColor(&u8g);
        u8g_SetFont(&u8g, u8g_font_7x13B);
    } while ( u8g_NextPage(&u8g) );
}

void printText(char *str)
{
    strncpy(textStr, str, CHARS_PER_ROW);
}

char *keyStrings[KEY_NUMBER_OF_KEYS] = {
    "KEY_NONE", "KEY8", "KEY9", "KEY0", "KEYSTART" ,
    "KEY6",     "KEYOK", "KEYDOWN", "KEY7" ,
    "KEYBACK",  "KEYUP", "KEY4",    "KEY5" ,
    "KEYHOME",  "KEY1",  "KEY2",    "KEY3"
};

void print_init_menu(void)
{
    //char buffer[64];
    u8g_FirstPage(&u8g);
    do {
        u8g_DrawStr(&u8g,  0, FONT_HEIGHT, textStr);
        //sprintf(buffer, "%ld", systick_cnt);
        u8g_DrawStr(&u8g,  0, FONT_HEIGHT*2, "1=SENSORS 2=GPIO");
        u8g_DrawStr(&u8g,  0, FONT_HEIGHT*3, "3=MOTOR");
    } while ( u8g_NextPage(&u8g) );
}

void print_sensors_menu(void)
{
    char buffer[64];
    u8g_FirstPage(&u8g);
    do {
        u8g_DrawStr(&u8g,  0, FONT_HEIGHT, textStr);
        //sprintf(buffer, "key 0x%08lX", getKeyboardDebugvalue());
        //u8g_DrawStr(&u8g,  0, FONT_HEIGHT*3, buffer);
        sprintf(buffer, "LIFT=%d FRONT=%d", (int)get_sensor(SENSOR_LIFT), (int)get_sensor(SENSOR_FRONT));
        u8g_DrawStr(&u8g,  0, FONT_HEIGHT*3, buffer);

        //sprintf(buffer, "key %d", get_pressed_key());
        //u8g_DrawStr(&u8g,  0, FONT_HEIGHT*4, buffer);
        u8g_DrawStr(&u8g,  0, FONT_HEIGHT*4, keyStrings[get_pressed_key()]);
    } while ( u8g_NextPage(&u8g) );
}

void print_gpio_menu(void)
{
    char buffer[64];
    u8g_FirstPage(&u8g);
    do {
        sprintf(buffer, "GPIO1 0x%08lX", LPC_GPIO0->FIOPIN);
        u8g_DrawStr(&u8g,  0, FONT_HEIGHT*1, buffer);
        sprintf(buffer, "GPIO1 0x%08lX", LPC_GPIO1->FIOPIN);
        u8g_DrawStr(&u8g,  0, FONT_HEIGHT*2, buffer);
        sprintf(buffer, "GPIO2 0x%08lX", LPC_GPIO2->FIOPIN);
        u8g_DrawStr(&u8g,  0, FONT_HEIGHT*3, buffer);
        sprintf(buffer, "GPIO3 0x%08lX", LPC_GPIO3->FIOPIN);
        u8g_DrawStr(&u8g,  0, FONT_HEIGHT*4, buffer);
        sprintf(buffer, "GPIO4 0x%08lX", LPC_GPIO4->FIOPIN);
        u8g_DrawStr(&u8g,  0, FONT_HEIGHT*5, buffer);
    } while ( u8g_NextPage(&u8g) );
}
/*
//debug
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

#define RIGHT_DIRECTION_PORTNO   2 
#define RIGHT_DIRECTION_PINNO    6 // 1=FORWARD, 0=BACKWARDS
#define LEFT_DIRECTION_PORTNO    0
#define LEFT_DIRECTION_PINNO     0
#define SPINDLE_DIRECTION_PORTNO 3
#define SPINDLE_DIRECTION_PINNO  26

static uint8_t brake=0, enable=0, pwm=0, direction=0;
//end debug 
*/
void print_motor_menu(void)
{
    char buffer[64];
    u8g_FirstPage(&u8g);
    do {  
        sprintf(buffer, "1 RIGHT   %d", rightspeed);
        u8g_DrawStr(&u8g,  0, FONT_HEIGHT*1, buffer);
        sprintf(buffer, "2 LEFT    %d", leftspeed);
        u8g_DrawStr(&u8g,  0, FONT_HEIGHT*2, buffer);
        sprintf(buffer, "3 SPINDLE %d", spindlespeed);
        u8g_DrawStr(&u8g,  0, FONT_HEIGHT*3, buffer);
        /*
        sprintf(buffer, "brake %d enable %d", brake, enable);
        u8g_DrawStr(&u8g,  0, FONT_HEIGHT*1, buffer);
        sprintf(buffer, "pwm %d dir %d", pwm, direction);
        u8g_DrawStr(&u8g,  0, FONT_HEIGHT*2, buffer);
*/
    } while ( u8g_NextPage(&u8g) );

}

typedef enum {
    taskstate_init = 0,
    taskstate_debugsensors,
    taskstate_debuggpio,
    taskstate_debugmotors,

    taskstate_number_of_states
}taskstate_t;

void task_display(void) {
    static taskstate_t taskstate;
    static keys_t lastpressedkey=0;
    keys_t currentpressedkey;
    currentpressedkey = get_pressed_key();

    switch(taskstate) {
        case taskstate_init:
            print_init_menu();
            if(lastpressedkey==KEY_NONE) {
                if(currentpressedkey==KEY1) {
                    taskstate = taskstate_debugsensors;
                }
                if(currentpressedkey==KEY2) {
                    taskstate = taskstate_debuggpio;
                }
                if(currentpressedkey==KEY3) {
                    taskstate = taskstate_debugmotors;
                }
            }
            break;
        case taskstate_debugsensors:
            print_sensors_menu();
            if(lastpressedkey==KEY_NONE) {
                if(currentpressedkey==KEYBACK) {
                    taskstate = taskstate_init;
                }
            }
            break;
        case taskstate_debuggpio:
            print_gpio_menu();
            if(lastpressedkey==KEY_NONE) {
                if(currentpressedkey==KEYBACK) {
                    taskstate = taskstate_init;
                }
            }
            break;
        case taskstate_debugmotors:
            print_motor_menu();
            if(lastpressedkey==KEY_NONE) {
                if(currentpressedkey==KEYBACK) {
                    taskstate = taskstate_init;
                }

/* debug function. runs motor when brake=0, enable=1, pwm=1, direction 1=forwards 
                if(currentpressedkey==KEY1) {
                    brake ^= 1;
                }
                if(currentpressedkey==KEY2) {
                    enable ^= 1;
                }
                if(currentpressedkey==KEY3) {
                    pwm ^= 1;
                }
                if(currentpressedkey==KEY4) {
                    direction ^= 1;
                }
                if(brake) {
                    LPC_GPIOx(LEFT_BRAKE_PORTNO)->FIOSET = ( 1 << LEFT_BRAKE_PINNO);
                } else {
                    LPC_GPIOx(LEFT_BRAKE_PORTNO)->FIOCLR = ( 1 << LEFT_BRAKE_PINNO);
                }
                if(enable) {
                    LPC_GPIOx(LEFT_ENABLE_PORTNO)->FIOSET = ( 1 << LEFT_ENABLE_PINNO);
                } else {
                    LPC_GPIOx(LEFT_ENABLE_PORTNO)->FIOCLR = ( 1 << LEFT_ENABLE_PINNO);
                }
                if(pwm) {
                    LPC_GPIOx(LEFT_PWM_PORTNO)->FIOSET = ( 1 << LEFT_PWM_PINNO);
                } else {
                    LPC_GPIOx(LEFT_PWM_PORTNO)->FIOCLR = ( 1 << LEFT_PWM_PINNO);
                }
                if(direction) {
                    LPC_GPIOx(LEFT_DIRECTION_PORTNO)->FIOSET = ( 1 << LEFT_DIRECTION_PINNO);
                } else {
                    LPC_GPIOx(LEFT_DIRECTION_PORTNO)->FIOCLR = ( 1 << LEFT_DIRECTION_PINNO);                    
                }

*/
/*
// runs right motor when brake=1 and enable=0 and pwm=1. direction controls direction
                if(currentpressedkey==KEY1) {
                    brake ^= 1;
                }
                if(currentpressedkey==KEY2) {
                    enable ^= 1;
                }
                if(currentpressedkey==KEY3) {
                    pwm ^= 1;
                }
                if(currentpressedkey==KEY4) {
                    direction ^= 1;
                }
                if(brake) {
                    LPC_GPIOx(RIGHT_BRAKE_PORTNO)->FIOSET = ( 1 << RIGHT_BRAKE_PINNO);
                } else {
                    LPC_GPIOx(RIGHT_BRAKE_PORTNO)->FIOCLR = ( 1 << RIGHT_BRAKE_PINNO);
                }
                if(enable) {
                    LPC_GPIOx(RIGHT_ENABLE_PORTNO)->FIOSET = ( 1 << RIGHT_ENABLE_PINNO);
                } else {
                    LPC_GPIOx(RIGHT_ENABLE_PORTNO)->FIOCLR = ( 1 << RIGHT_ENABLE_PINNO);
                }
                if(pwm) {
                    LPC_GPIOx(RIGHT_PWM_PORTNO)->FIOSET = ( 1 << RIGHT_PWM_PINNO);
                } else {
                    LPC_GPIOx(RIGHT_PWM_PORTNO)->FIOCLR = ( 1 << RIGHT_PWM_PINNO);
                }
                if(direction) {
                    LPC_GPIOx(RIGHT_DIRECTION_PORTNO)->FIOSET = ( 1 << RIGHT_DIRECTION_PINNO);
                } else {
                    LPC_GPIOx(RIGHT_DIRECTION_PORTNO)->FIOCLR = ( 1 << RIGHT_DIRECTION_PINNO);                    
                }
*/
/*
// runs spindle motor when brake=1 and enable=0 and pwm=1. direction controls direction
// brake=0 stops motor, 1 free runs motor
                if(currentpressedkey==KEY1) {
                    brake ^= 1;
                }
                if(currentpressedkey==KEY2) {
                    enable ^= 1;
                }
                if(currentpressedkey==KEY3) {
                    pwm ^= 1;
                }
                if(currentpressedkey==KEY4) {
                    direction ^= 1;
                }
                if(brake) {
                    LPC_GPIOx(SPINDLE_BRAKE_PORTNO)->FIOSET = ( 1 << SPINDLE_BRAKE_PINNO);
                } else {
                    LPC_GPIOx(SPINDLE_BRAKE_PORTNO)->FIOCLR = ( 1 << SPINDLE_BRAKE_PINNO);
                }
                if(enable) {
                    LPC_GPIOx(SPINDLE_ENABLE_PORTNO)->FIOSET = ( 1 << SPINDLE_ENABLE_PINNO);
                } else {
                    LPC_GPIOx(SPINDLE_ENABLE_PORTNO)->FIOCLR = ( 1 << SPINDLE_ENABLE_PINNO);
                }
                if(pwm) {
                    LPC_GPIOx(SPINDLE_PWM_PORTNO)->FIOSET = ( 1 << SPINDLE_PWM_PINNO);
                } else {
                    LPC_GPIOx(SPINDLE_PWM_PORTNO)->FIOCLR = ( 1 << SPINDLE_PWM_PINNO);
                }
                if(direction) {
                    LPC_GPIOx(SPINDLE_DIRECTION_PORTNO)->FIOSET = ( 1 << SPINDLE_DIRECTION_PINNO);
                } else {
                    LPC_GPIOx(SPINDLE_DIRECTION_PORTNO)->FIOCLR = ( 1 << SPINDLE_DIRECTION_PINNO);                    
                }
*/


                if(currentpressedkey==KEY1) {
                    rightspeed=(rightspeed+10)%100;
                    set_motor_speed(MOTOR_RIGHT, rightspeed);
                }
                if(currentpressedkey==KEY2) {
                    leftspeed=(leftspeed+10)%100;
                    set_motor_speed(MOTOR_LEFT, leftspeed);
                }
                if(currentpressedkey==KEY3) {
                    spindlespeed=(spindlespeed+10)%100;
                    set_motor_speed(MOTOR_SPINDLE, spindlespeed);
                }
                if(currentpressedkey==KEY4) {
                    set_motor_speed(MOTOR_RIGHT, 0);
                }
                if(currentpressedkey==KEY5) {
                    set_motor_speed(MOTOR_LEFT, 0);
                }
                if(currentpressedkey==KEY6) {
                    set_motor_speed(MOTOR_SPINDLE, 0);
                }

                
                /* starts left motor when 2 is pressed 
                if(currentpressedkey==KEY1) {
                    LPC_GPIOx(RIGHT_DIRECTION_PORTNO)->FIODIR |= ( 1 << RIGHT_DIRECTION_PINNO);
                    LPC_GPIOx(RIGHT_DIRECTION_PORTNO)->FIOSET = ( 1 << RIGHT_DIRECTION_PINNO);

                    LPC_GPIOx(RIGHT_BRAKE_PORTNO)->FIODIR |= ( 1 << RIGHT_BRAKE_PINNO);
                    LPC_GPIOx(RIGHT_BRAKE_PORTNO)->FIOCLR = ( 1 << RIGHT_BRAKE_PINNO);

                    LPC_GPIOx(RIGHT_ENABLE_PORTNO)->FIODIR |= ( 1 << RIGHT_ENABLE_PINNO);
                    LPC_GPIOx(RIGHT_ENABLE_PORTNO)->FIOSET = ( 1 << RIGHT_ENABLE_PINNO);
                }
                if(currentpressedkey==KEY2) {
                    LPC_GPIOx(LEFT_DIRECTION_PORTNO)->FIODIR |= ( 1 << LEFT_DIRECTION_PINNO);
                    LPC_GPIOx(LEFT_DIRECTION_PORTNO)->FIOSET = ( 1 << LEFT_DIRECTION_PINNO);

                    LPC_GPIOx(LEFT_BRAKE_PORTNO)->FIODIR |= ( 1 << LEFT_BRAKE_PINNO);
                    LPC_GPIOx(LEFT_BRAKE_PORTNO)->FIOCLR = ( 1 << LEFT_BRAKE_PINNO);

                    LPC_GPIOx(LEFT_ENABLE_PORTNO)->FIODIR |= ( 1 << LEFT_ENABLE_PINNO);
                    LPC_GPIOx(LEFT_ENABLE_PORTNO)->FIOSET = ( 1 << LEFT_ENABLE_PINNO);
                }
                if(currentpressedkey==KEY3) {
                    LPC_GPIOx(SPINDLE_DIRECTION_PORTNO)->FIODIR |= ( 1 << SPINDLE_DIRECTION_PINNO);
                    LPC_GPIOx(SPINDLE_DIRECTION_PORTNO)->FIOSET = ( 1 << SPINDLE_DIRECTION_PINNO);

                    LPC_GPIOx(SPINDLE_BRAKE_PORTNO)->FIODIR |= ( 1 << SPINDLE_BRAKE_PINNO);
                    LPC_GPIOx(SPINDLE_BRAKE_PORTNO)->FIOCLR = ( 1 << SPINDLE_BRAKE_PINNO);

                    LPC_GPIOx(SPINDLE_ENABLE_PORTNO)->FIODIR |= ( 1 << SPINDLE_ENABLE_PINNO);
                    LPC_GPIOx(SPINDLE_ENABLE_PORTNO)->FIOSET = ( 1 << SPINDLE_ENABLE_PINNO);
                }
                */
                /* Pressing 3 starts left motor 
                if(currentpressedkey==KEY1) {
                    LPC_GPIOx(LEFT_DIRECTION_PORTNO)->FIODIR |= ( 1 << LEFT_DIRECTION_PINNO);
                    LPC_GPIOx(LEFT_DIRECTION_PORTNO)->FIOSET = ( 1 << LEFT_DIRECTION_PINNO);
                }
                if(currentpressedkey==KEY2) {
                    LPC_GPIOx(LEFT_BRAKE_PORTNO)->FIODIR |= ( 1 << LEFT_BRAKE_PINNO);
                    LPC_GPIOx(LEFT_BRAKE_PORTNO)->FIOCLR = ( 1 << LEFT_BRAKE_PINNO);
                }
                if(currentpressedkey==KEY3) {
                    LPC_GPIOx(LEFT_ENABLE_PORTNO)->FIODIR |= ( 1 << LEFT_ENABLE_PINNO);
                    LPC_GPIOx(LEFT_ENABLE_PORTNO)->FIOSET = ( 1 << LEFT_ENABLE_PINNO);
                }
                if(currentpressedkey==KEY4) {
                    LPC_GPIOx(LEFT_ENABLE_PORTNO)->FIOCLR = ( 1 << LEFT_ENABLE_PINNO);
                }
                */
            }
            break;
        default:
            DOASSERT();
            break;
    }
    lastpressedkey = currentpressedkey;
}

/*


#define PWM_COUNTER_MAXVALUE 1000 // 2kHz

void init_hal_motor(void) {
  
  LPC_SC->PCONP |= (1 << 6);   // power up PWM1

  // Note that these values must be updated if x_PWM_PINNO or PORTNO changes
  LPC_PINCON->PINSEL4 &= ~0x3f; // Reset all bits of port2.0-port2.2
  LPC_PINCON->PINSEL4 |= 0x2a;  // Set port2.0-port2.2 to alternate function 01 (PWM)

  LPC_PWM1->PCR &= ~(0x0e00); // Disable pwm 1-3
  LPC_PWM1->PR = 12; // The TC is incremented every PR+1 cycles of PCLK.
  LPC_PWM1->MR0 = PWM_COUNTER_MAXVALUE; // 2khz
  LPC_PWM1->MR1 = 0; // PWM1
  LPC_PWM1->MR2 = 0; // PWM2
  LPC_PWM1->MR3 = 0; // PWM3
  LPC_PWM1->MCR = 0x02; // Reset Timer1 when MR0 matches timer counter
  LPC_PWM1->LER = 0x0f; // Update MR0-MR3 on next timer1 reset. Note that this write disables update of other MR-registers if the bits were set

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

}

*/