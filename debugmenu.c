#include "system.h"
#include <stdio.h>
#include <string.h>
#include "hal/hal_keyboard.h"
#include "hal/hal_sensors.h"
#include "hal/hal_motor.h"
#include "display.h"

#include "hal/hal_mcu.h"//debug

static uint8_t rightspeed=0, leftspeed=0, spindlespeed=0;
static bool menuactive;
typedef enum {
    taskstate_init = 0,
    taskstate_debugsensors,
    taskstate_debuggpio,
    taskstate_debugmotors,
    taskstate_inactive,

    taskstate_number_of_states
}taskstate_t;
static taskstate_t taskstate;

bool is_debugmenu_active(void){
    return menuactive;
}

void init_debugmenu(void) {
    menuactive = true;
    taskstate = taskstate_init;
}

char *keyStrings[KEY_NUMBER_OF_KEYS] = {
    "KEY_NONE", "KEY8", "KEY9", "KEY0", "KEYSTART" ,
    "KEY6",     "KEYOK", "KEYDOWN", "KEY7" ,
    "KEYBACK",  "KEYUP", "KEY4",    "KEY5" ,
    "KEYHOME",  "KEY1",  "KEY2",    "KEY3"
};

static void print_init_menu(void)
{
    char buffer[64];

    print_text(0, "DEBUGMENU");
    print_text(1, "1=SENSORS 2=GPIO");
    print_text(2, "3=MOTOR");
    sprintf(buffer, "%ld", systick_cnt);
    print_text(3, buffer);
}

static void print_sensors_menu(void)
{
    char buffer[64];

    sprintf(buffer, "L %s R %s", get_sensor(SENSOR_LEFT_WIRE_INSIDE) ? "IN " : "OUT", get_sensor(SENSOR_RIGHT_WIRE_INSIDE) ? "IN " : "OUT");
    print_text(0, buffer);
    sprintf(buffer, "LIFT=%d FRONT=%d", (int)get_sensor(SENSOR_LIFT), (int)get_sensor(SENSOR_FRONT));
    print_text(1, buffer);
    if(get_sensor(SENSOR_STOPBTN)) {
        print_text(2, "KEYSTOP");
    } else {
        print_text(2, keyStrings[get_pressed_key()]);
    }

    trigger_wire_sensor();
}

static void print_gpio_menu(void)
{
    char buffer[64];
    sprintf(buffer, "GPIO1 0x%08lX", LPC_GPIO0->FIOPIN);
    print_text(0, buffer);
    sprintf(buffer, "GPIO1 0x%08lX", LPC_GPIO1->FIOPIN);
    print_text(1, buffer);
    sprintf(buffer, "GPIO2 0x%08lX", LPC_GPIO2->FIOPIN);
    print_text(2, buffer);
    sprintf(buffer, "GPIO3 0x%08lX", LPC_GPIO3->FIOPIN);
    print_text(3, buffer);
    sprintf(buffer, "GPIO4 0x%08lX", LPC_GPIO4->FIOPIN);
    print_text(4, buffer);
}

static void print_motor_menu(void)
{
    char buffer[64];

    sprintf(buffer, "1 RIGHT   %d", rightspeed);
    print_text(0, buffer);
    sprintf(buffer, "2 LEFT    %d", leftspeed);
    print_text(1, buffer);
    sprintf(buffer, "3 SPINDLE %d", spindlespeed);
    print_text(2, buffer);
}

void task_debugmenu(void) {
    static keys_t lastpressedkey=0;
    keys_t currentpressedkey;
    currentpressedkey = get_pressed_key();

    switch(taskstate) {
        case taskstate_init:
            print_init_menu();
            if(lastpressedkey==KEY_NONE) {
                if(currentpressedkey==KEY1) {
                    clear_display();
                    taskstate = taskstate_debugsensors;
                }
                if(currentpressedkey==KEY2) {
                    clear_display();
                    taskstate = taskstate_debuggpio;
                }
                if(currentpressedkey==KEY3) {
                    clear_display();
                    taskstate = taskstate_debugmotors;
                }
                if(currentpressedkey==KEYBACK) {
                    clear_display();
                    menuactive=false;
                    taskstate = taskstate_inactive;
                }
            }
            break;
        case taskstate_debugsensors:
            print_sensors_menu();
            if(lastpressedkey==KEY_NONE) {
                if(currentpressedkey==KEYBACK) {
                    clear_display();
                    taskstate = taskstate_init;
                }
            }
            break;
        case taskstate_debuggpio:
            print_gpio_menu();
            if(lastpressedkey==KEY_NONE) {
                if(currentpressedkey==KEYBACK) {
                    clear_display();
                    taskstate = taskstate_init;
                }
            }
            break;
        case taskstate_debugmotors:
            print_motor_menu();
            if(lastpressedkey==KEY_NONE) {
                if(currentpressedkey==KEYBACK) {
                    clear_display();
                    taskstate = taskstate_init;
                }
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
            }
            break;
        case taskstate_inactive:
            break;
        default:
            DOASSERT();
            break;
    }
    lastpressedkey = currentpressedkey;
}