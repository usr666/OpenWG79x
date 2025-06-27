#include "system.h"
#include <stdio.h>
#include <string.h>
#include "hal/hal_keyboard.h"
#include "hal/hal_sensors.h"
#include "hal/hal_motor.h"
#include "hal/hal_charger.h"
#include "display.h"
#include "debugmenu.h"

static char *stopreason;

typedef enum {
    mainstate_idle = 0,
    mainstate_debug,
    mainstate_mow,
    mainstate_charging,
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
    mowstate_turn,
    mowstate_turn_2,
    mowstate_turn_3,
    mowstate_backoff,
    mowstate_backoff_2,
    mowstate_backoff_3,
    mowstate_out_of_area,
    mowstate_out_of_area_2
}mowstate_t;
static mowstate_t mowstate;
static bool turnleft;

void init_mow(void) {
    mowstate = mowstate_startmow;
}

#define SLOW_SPEED      20
#define DEFAULT_SPEED   45
#define SPINDLE_DEFAULT_SPEED 80
#define SLOW_RAMP       10
#define DEFAULT_RAMP    20
#define FAST_RAMP       30
#define IMMEDIATE_RAMP 100
#define TURN_TIME_MS    2000
#define BACKOFF_TIME_MS 1200
#define MAX_TIME_OUT_OF_AREA 4000

void stop_all_motors(void) {
    set_motor_ramp(MOTOR_RIGHT, IMMEDIATE_RAMP);
    set_motor_ramp(MOTOR_LEFT, IMMEDIATE_RAMP);
    set_motor_ramp(MOTOR_SPINDLE, IMMEDIATE_RAMP);
    set_motor_speed(MOTOR_RIGHT, 0);
    set_motor_speed(MOTOR_LEFT, 0);
    set_motor_speed(MOTOR_SPINDLE, 0);
}

void checksensors(void) {
    if(get_charger_connected()) {
        mainstate = mainstate_charging;
    } else if(get_sensor(SENSOR_LEFT_WIRE_INSIDE) == false) {
        turnleft = false;
        mowstate = mowstate_turn;
    } else if(get_sensor(SENSOR_RIGHT_WIRE_INSIDE) == false) {
        turnleft = true;
        mowstate = mowstate_turn;
    } else if(get_sensor(SENSOR_LIFT)) {
        set_motor_ramp(MOTOR_SPINDLE, IMMEDIATE_RAMP);
        set_motor_speed(MOTOR_SPINDLE, 0);
        mowstate = mowstate_backoff;
    } else if(get_sensor(SENSOR_FRONT)) {
        mowstate = mowstate_backoff;
    }
}

void mow_state(void) {
    static systimer_t mowtimer, timeouttimer;
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
            }
            break;
        case mowstate_running:
            set_motor_ramp(MOTOR_RIGHT, DEFAULT_RAMP);
            set_motor_ramp(MOTOR_LEFT, DEFAULT_RAMP);
            set_motor_ramp(MOTOR_SPINDLE, DEFAULT_RAMP);
            set_motor_speed(MOTOR_RIGHT, DEFAULT_SPEED);
            set_motor_speed(MOTOR_LEFT, DEFAULT_SPEED);
            set_motor_speed(MOTOR_SPINDLE, SPINDLE_DEFAULT_SPEED);
            checksensors();
            break;
        case mowstate_out_of_area:
            set_motor_ramp(MOTOR_RIGHT, FAST_RAMP);
            set_motor_ramp(MOTOR_LEFT, FAST_RAMP);
            set_motor_speed(MOTOR_RIGHT, -SLOW_SPEED);
            set_motor_speed(MOTOR_LEFT, -SLOW_SPEED);
            systimer_start(&timeouttimer, MAX_TIME_OUT_OF_AREA);
            mowstate = mowstate_out_of_area_2;
            break;
        case mowstate_out_of_area_2:
            if(get_sensor(SENSOR_RIGHT_WIRE_INSIDE) || get_sensor(SENSOR_LEFT_WIRE_INSIDE)) {
                mowstate = mowstate_running;
            }
            if(systimer_is_expired(&timeouttimer)) {
                mainstate = mainstate_stopped;
                stopreason = "Out of area";
            }
            break;
        case mowstate_turn:
            set_motor_ramp(MOTOR_RIGHT, FAST_RAMP);
            set_motor_ramp(MOTOR_LEFT, FAST_RAMP);
            if(turnleft) {
                set_motor_speed(MOTOR_RIGHT, SLOW_SPEED);
                set_motor_speed(MOTOR_LEFT, -SLOW_SPEED);
            } else {
                set_motor_speed(MOTOR_RIGHT, -SLOW_SPEED);
                set_motor_speed(MOTOR_LEFT, SLOW_SPEED);
            }
            systimer_start(&mowtimer, TURN_TIME_MS);
            systimer_start(&timeouttimer, TURN_TIME_MS*4);
            mowstate = mowstate_turn_2;
            break;
        case mowstate_turn_2:
            if(systimer_is_expired(&mowtimer)) {
                bool othersensor;
                if(turnleft) {
                    othersensor = get_sensor(SENSOR_RIGHT_WIRE_INSIDE);
                } else {
                    othersensor = get_sensor(SENSOR_LEFT_WIRE_INSIDE);
                }
                if(othersensor == false) {
                    if(systimer_is_expired(&timeouttimer)) {
                        mainstate = mainstate_stopped;
                        stopreason = "Timeout in turn";
                    }
                } else {
                    set_motor_speed(MOTOR_RIGHT, DEFAULT_SPEED);
                    set_motor_speed(MOTOR_LEFT, DEFAULT_SPEED);
                    mowstate = mowstate_turn_3;
                    systimer_start(&mowtimer, TURN_TIME_MS);
                }
            }
            break;
        case mowstate_turn_3:
            checksensors();
            if(systimer_is_expired(&mowtimer)) {
                if(get_sensor(SENSOR_RIGHT_WIRE_INSIDE) == false && get_sensor(SENSOR_LEFT_WIRE_INSIDE) == false) {
                    mowstate = mowstate_out_of_area;
                } else {
                    mowstate = mowstate_running;
                }
            }
            break;
        case mowstate_backoff:
            set_motor_ramp(MOTOR_RIGHT, IMMEDIATE_RAMP);
            set_motor_ramp(MOTOR_LEFT, IMMEDIATE_RAMP);
            set_motor_speed(MOTOR_RIGHT, 0);
            set_motor_speed(MOTOR_LEFT, 0);
            mowstate = mowstate_backoff_2;
            break;
        case mowstate_backoff_2:
            set_motor_ramp(MOTOR_RIGHT, FAST_RAMP);
            set_motor_ramp(MOTOR_LEFT, FAST_RAMP);
            set_motor_speed(MOTOR_RIGHT, -SLOW_SPEED);
            set_motor_speed(MOTOR_LEFT, -SLOW_SPEED);
            systimer_start(&mowtimer, BACKOFF_TIME_MS);
            mowstate = mowstate_backoff_3;
            break;
        case mowstate_backoff_3:
            if(systimer_is_expired(&mowtimer)) {
                set_motor_speed(MOTOR_RIGHT, 0);
                set_motor_speed(MOTOR_LEFT, 0);
                turnleft = true;
                mowstate = mowstate_turn;
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
            stop_all_motors();
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
        case mainstate_charging:
            stop_all_motors();
            clear_display();
            print_text(0, "Charging");
            if(lastpressedkey==KEY_NONE && currentpressedkey==KEYBACK) {
                mainstate = mainstate_idle;
            }
            if(get_charger_connected() == false) {
                mainstate = mainstate_stopped;
                stopreason = "Charger disconnected";
            }
            break;
        case mainstate_stopped:
            stop_all_motors();
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