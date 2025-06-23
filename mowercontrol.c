#include "system.h"
#include <stdio.h>
#include <string.h>
#include "hal/hal_keyboard.h"
#include "hal/hal_sensors.h"
#include "hal/hal_motor.h"
#include "display.h"
#include "debugmenu.h"

static char *stopreason;

typedef enum {
    mainstate_idle = 0,
    mainstate_debug,
    mainstate_mow,
    mainstate_stopped,
    mainstate_number_of_states
}mainstate_t;
static mainstate_t mainstate;

void init_mowercontrol(void) {
    mainstate=mainstate_idle;
    stopreason="power on";
}

static void print_init_menu(void)
{
    char buffer[64];

    print_text(0, "OpenWG79x");
    print_text(1, "Press START to mow");
    print_text(2, "Press 2 to debug");
    sprintf(buffer, "%ld", systick_cnt);
    print_text(3, buffer);
}

typedef enum {
    mowstate_startmow = 0,
    mowstate_running,
    mowstate_turnright,
    mowstate_turnleft,
    mowstate_turnright_2,
    mowstate_turnleft_2,
    mowstate_backoff,
    mowstate_backoff_2,
    mowstate_backoff_3
}mowstate_t;
static mowstate_t mowstate;

void init_mow(void) {
    mowstate = mowstate_startmow;
}

#define DEFAULT_SPEED 45
#define DEFAULT_RAMP 10
#define TURN_TIME_MS 1000
#define BACKOFF_TIME_MS 1000

void mow_state(void) {
    static systimer_t mowtimer;
    if(get_sensor(SENSOR_STOPBTN)) {
        mainstate = mainstate_stopped;
        stopreason="Stopbtn pressed";
    }
    switch(mowstate) {
        case mowstate_startmow:
            if(get_sensor(SENSOR_RIGHT_WIRE_INSIDE) == false || get_sensor(SENSOR_LEFT_WIRE_INSIDE) == false) {
                mainstate = mainstate_stopped;
                stopreason = "Out of area";
            } else {
                mowstate = mowstate_running;
                set_motor_ramp(MOTOR_RIGHT, DEFAULT_RAMP);
                set_motor_ramp(MOTOR_LEFT, DEFAULT_RAMP);
            }
            break;
        case mowstate_running:
            set_motor_speed(MOTOR_RIGHT, DEFAULT_SPEED);
            set_motor_speed(MOTOR_LEFT, DEFAULT_SPEED);
            if(get_sensor(SENSOR_LEFT_WIRE_INSIDE) == false) {
                mowstate = mowstate_turnright;
            } else if(get_sensor(SENSOR_RIGHT_WIRE_INSIDE) == false) {
                mowstate = mowstate_turnleft;
            } else if(get_sensor(SENSOR_LIFT)) {
                set_motor_ramp(MOTOR_SPINDLE, 100);
                set_motor_speed(MOTOR_SPINDLE, 0);
                mowstate = mowstate_backoff;
            } else if(get_sensor(SENSOR_FRONT)) {
                mowstate = mowstate_backoff;
            }
            break;
        case mowstate_turnleft:
            set_motor_speed(MOTOR_RIGHT, DEFAULT_SPEED);
            set_motor_speed(MOTOR_LEFT, -DEFAULT_SPEED);
            systimer_start(&mowtimer, TURN_TIME_MS);
            mowstate = mowstate_turnleft_2;
            break;
        case mowstate_turnleft_2:
            if(systimer_is_expired(&mowtimer)) {
                if(get_sensor(SENSOR_RIGHT_WIRE_INSIDE) == false && get_sensor(SENSOR_LEFT_WIRE_INSIDE) == false) {
                    mainstate = mainstate_stopped;
                    stopreason = "Out of area";
                } else {
                    set_motor_speed(MOTOR_RIGHT, DEFAULT_SPEED);
                    set_motor_speed(MOTOR_LEFT, DEFAULT_SPEED);
                    mowstate = mowstate_running;
                }
            }
            break;
        case mowstate_turnright:
            set_motor_speed(MOTOR_RIGHT, -DEFAULT_SPEED);
            set_motor_speed(MOTOR_LEFT, DEFAULT_SPEED);
            systimer_start(&mowtimer, TURN_TIME_MS);
            mowstate = mowstate_turnright_2;
            break;
        case mowstate_turnright_2:
            if(systimer_is_expired(&mowtimer)) {
                if(get_sensor(SENSOR_RIGHT_WIRE_INSIDE) == false && get_sensor(SENSOR_LEFT_WIRE_INSIDE) == false) {
                    mainstate = mainstate_stopped;
                    stopreason = "Out of area";
                } else {
                    set_motor_speed(MOTOR_RIGHT, DEFAULT_SPEED);
                    set_motor_speed(MOTOR_LEFT, DEFAULT_SPEED);
                    mowstate = mowstate_running;
                }
            }
            break;
        case mowstate_backoff:
            set_motor_ramp(MOTOR_RIGHT, 100);
            set_motor_ramp(MOTOR_LEFT, 100);
            set_motor_speed(MOTOR_RIGHT, 0);
            set_motor_speed(MOTOR_LEFT, 0);
            mowstate = mowstate_backoff_2;
            break;
        case mowstate_backoff_2:
            set_motor_ramp(MOTOR_RIGHT, DEFAULT_RAMP);
            set_motor_ramp(MOTOR_LEFT, DEFAULT_RAMP);
            set_motor_speed(MOTOR_RIGHT, -DEFAULT_SPEED);
            set_motor_speed(MOTOR_LEFT, -DEFAULT_SPEED);
            systimer_start(&mowtimer, BACKOFF_TIME_MS);
            mowstate = mowstate_backoff_3;
            break;
        case mowstate_backoff_3:
            if(systimer_is_expired(&mowtimer)) {
                set_motor_speed(MOTOR_RIGHT, 0);
                set_motor_speed(MOTOR_LEFT, 0);
                mowstate = mowstate_turnleft;
            }
            break;
    }
}

void task_mowercontrol(void) {
    static keys_t lastpressedkey=0;
    keys_t currentpressedkey;
    currentpressedkey = get_pressed_key();

    switch(mainstate) {
        case mainstate_idle:
            print_init_menu();
            if(lastpressedkey==KEY_NONE) {
                if(currentpressedkey==KEY2) {
                    clear_display();
                    init_debugmenu();
                    mainstate = mainstate_debug;
                }
                if(currentpressedkey==KEYSTART) {
                    clear_display();
                    print_text(0, "Mowing...");
                    mainstate = mainstate_mow;
                    init_mow();
                }
            }
            break;
        case mainstate_mow:
            mow_state();
            break;
        case mainstate_debug:
            if(is_debugmenu_active()) {
                task_debugmenu();
            } else {
                clear_display();
                mainstate = mainstate_idle;
            }
            break;
        case mainstate_stopped:
            set_motor_ramp(MOTOR_RIGHT, 100);
            set_motor_ramp(MOTOR_LEFT, 100);
            set_motor_ramp(MOTOR_SPINDLE, 100);
            set_motor_speed(MOTOR_RIGHT, 0);
            set_motor_speed(MOTOR_LEFT, 0);
            set_motor_speed(MOTOR_SPINDLE, 0);
            clear_display();
            print_text(0, "Stopped");
            print_text(1, stopreason);
            print_text(2, "Press back");
            if(lastpressedkey==KEY_NONE && currentpressedkey==KEYBACK) {
                mainstate = mainstate_idle;
            }
            break;
        default:
            DOASSERT();
            break;
    }
    lastpressedkey = currentpressedkey;
}