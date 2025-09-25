#include "system.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "hal/hal_keyboard.h"
#include "hal/hal_sensors.h"
#include "hal/hal_motor.h"
#include "hal/hal_charger.h"
#include "hal/hal_power.h"
#include "display.h"
#include "debugmenu.h"

static char *stopreason;
static bool mowing = false;

typedef enum {
    mainstate_idle = 0,
    mainstate_debug,
    mainstate_mow,
    mainstate_startcharge,
    mainstate_charging,
    mainstate_stopped,
    mainstate_stopped_2,
    mainstate_number_of_states
}mainstate_t;
static mainstate_t mainstate;

typedef enum {
    mowstate_startmow = 0,
    mowstate_running,
    mowstate_running_downhill,
    mowstate_turn,
    mowstate_turn_2,
    mowstate_turn_3,
    mowstate_backoff,
    mowstate_backoff_2,
    mowstate_backoff_3,
    mowstate_out_of_area,
    mowstate_out_of_area_2,
    mowstate_wire_found,           // 11
    mowstate_wire_found_slowturn,  // 12
    mowstate_wire_found_sharpturn, // 13
    mowstate_wire_found_turnright, // 14
    mowstate_wire_found_turnright_2,//15
    mowstate_refind_wire,
    mowstate_start_after_charge,
    mowstate_start_after_charge_2,
    mowstate_tilted
}mowstate_t;
static mowstate_t mowstate;
static bool turnleft; // Indicates turn direction if turning. true = turn left, false = turn right
static bool findhome; // true if we are looking for home position
static systimer_t lowsocpowerofftimer;
static uint8_t tiltcount;

#define SLOW_SPEED      20
#define INTERMEDIATE_SPEED 30
#define DEFAULT_SPEED   45
#define SPINDLE_DEFAULT_SPEED 80
#define SLOW_RAMP       10
#define DEFAULT_RAMP    20
#define FAST_RAMP       30
#define IMMEDIATE_RAMP 100
#define TURN_TIME_MS    2500
#define TURN_TIMEOUT_MS 10000
#define BACKOFF_TIME_MS 1200
#define WAIT_FOR_CHARGE_DETECT 4000
#define MAX_TIME_OUT_OF_AREA 4000
#define MAX_TIME_REFIND_WIRE 16000
#define REVERSE_AFTER_CHARGE_TIME_MS 5000
#define GO_TO_CHARGE_STATION_SOC 30
#define TURN_OFF_DISC_SOC 20
#define POWER_OFF_SOC 0
#define TIME_IN_LOW_SOC_BEFORE_POWEROFF 10000
#define TIME_IN_STOPPED_BEFORE_POWEROFF 300000
#define TILTED_ANGLE 45
#define TIME_BEFORE_MAX_TURN_IN_FIND_WIRE_MS 2000
#define TIME_TO_TURN_BACK_MS 300



void init_mowercontrol(void) {
    mainstate=mainstate_idle;
    stopreason="power on";
    findhome = false;
    systimer_start(&lowsocpowerofftimer, TIME_IN_LOW_SOC_BEFORE_POWEROFF);
}

static void print_init_menu(void)
{
    char buffer[64];
    uint32_t batteryvoltage;
    batteryvoltage = get_battery_voltage();
    sprintf(buffer, "%2ld.%1ldV %2d%%", batteryvoltage/1000, (batteryvoltage/100)%10, get_battery_soc());
    print_text(0, buffer);
    print_text(1, "Press START to mow");
    print_text(2, "Press 2 to debug");
    sprintf(buffer, "%ld", systick_cnt);
    print_text(3, buffer);
}

void stop_all_motors(void) {
    set_motor_ramp(MOTOR_RIGHT, IMMEDIATE_RAMP);
    set_motor_ramp(MOTOR_LEFT, IMMEDIATE_RAMP);
    set_motor_ramp(MOTOR_SPINDLE, IMMEDIATE_RAMP);
    set_motor_speed(MOTOR_RIGHT, 0);
    set_motor_speed(MOTOR_LEFT, 0);
    set_motor_speed(MOTOR_SPINDLE, 0);
}

/* Check sensors and update mainstate, mowstate and turnleft if needed */
void checksensors(bool wirefound) {
    if(get_charger_connected()) {
        mainstate = mainstate_startcharge;
    } else if(abs(get_pitch() > TILTED_ANGLE) || abs(get_roll() > TILTED_ANGLE)) {
        stop_all_motors();
        mowstate = mowstate_tilted;
    } else if(get_sensor(SENSOR_LIFT)) {
        set_motor_ramp(MOTOR_SPINDLE, IMMEDIATE_RAMP);
        set_motor_speed(MOTOR_SPINDLE, 0);
        mowstate = mowstate_backoff;
    } else if(get_sensor(SENSOR_FRONT)) {
        mowstate = mowstate_backoff;
    } else if(get_sensor(SENSOR_LEFT_WIRE_INSIDE) == false) {
        if(findhome) {
            if(!wirefound) {
                mowstate = mowstate_wire_found;
            }
        } else {
            turnleft = false;
            mowstate = mowstate_turn;
        }
    } else if(get_sensor(SENSOR_RIGHT_WIRE_INSIDE) == false) {
        if(findhome) {
            if(!wirefound) {
                mowstate = mowstate_wire_found;
            }
        } else {
            turnleft = true;
            mowstate = mowstate_turn;
        }
    }
}

/* Mow control state machine. Active when mainstate is mainstate_mow */
void mow_state(void) {
    static systimer_t mowtimer, timeouttimer;
    int roll, pitch;
    char tmpstr[20];
    uint32_t batteryvoltage;
    uint8_t soc;
    static bool last_left_sensor_inside, last_right_sensor_inside;
    bool left_sensor_inside, right_sensor_inside;
    batteryvoltage = get_battery_voltage();
    soc = get_battery_soc();

    clear_display();
    if(findhome) {
        print_text(0, "Finding home...");
    } else {
        print_text(0, "Mowing...");
    }

    sprintf(tmpstr, "%2ld.%1ldV %2d%%", batteryvoltage/1000, (batteryvoltage/100)%10, soc);
    print_text(1, tmpstr);

    if(get_sensor(SENSOR_STOPBTN)) {
        mainstate = mainstate_stopped;
        stopreason="Stopbtn pressed";
        return;
    }
    left_sensor_inside = get_sensor(SENSOR_LEFT_WIRE_INSIDE);
    right_sensor_inside = get_sensor(SENSOR_RIGHT_WIRE_INSIDE);

    switch(mowstate) {
        case mowstate_startmow:
            if(right_sensor_inside == false || left_sensor_inside == false) {
                mainstate = mainstate_stopped;
                stopreason = "Out of area";
            } else {
                mowstate = mowstate_running;
                mowing = true;
            }
            break;
        case mowstate_running:
            tiltcount = 0;
            set_motor_ramp(MOTOR_RIGHT, DEFAULT_RAMP);
            set_motor_ramp(MOTOR_LEFT, DEFAULT_RAMP);
            set_motor_ramp(MOTOR_SPINDLE, DEFAULT_RAMP);
            set_motor_speed(MOTOR_RIGHT, DEFAULT_SPEED);
            set_motor_speed(MOTOR_LEFT, DEFAULT_SPEED);
            roll = get_roll();
            pitch = get_pitch();
            if(pitch < -9) {
                mowstate = mowstate_running_downhill;
            }
            if(soc < TURN_OFF_DISC_SOC) {
                set_motor_speed(MOTOR_SPINDLE, 0);
            } else {
                set_motor_speed(MOTOR_SPINDLE, SPINDLE_DEFAULT_SPEED);
            }
            checksensors(false);
            if(soc < GO_TO_CHARGE_STATION_SOC) {
                findhome = true;
            }
            break;
        case mowstate_running_downhill:
            tiltcount = 0;
            set_motor_ramp(MOTOR_RIGHT, DEFAULT_RAMP);
            set_motor_ramp(MOTOR_LEFT, DEFAULT_RAMP);
            set_motor_ramp(MOTOR_SPINDLE, DEFAULT_RAMP);
            // If mower is going downhill go slow and make sure it does not go straight downhill
            roll = get_roll();
            pitch = get_pitch();
            if(pitch > 0) {
                mowstate = mowstate_running;
            } else {
                if(roll > 0) {
                    set_motor_speed(MOTOR_RIGHT, INTERMEDIATE_SPEED);
                    set_motor_speed(MOTOR_LEFT, INTERMEDIATE_SPEED/2);
                } else {
                    set_motor_speed(MOTOR_RIGHT, INTERMEDIATE_SPEED/2);
                    set_motor_speed(MOTOR_LEFT, INTERMEDIATE_SPEED);
                }
            }
            checksensors(false);
            break;
        case mowstate_refind_wire:
            if(systimer_is_expired(&timeouttimer)) {
                mowstate = mowstate_running;
            }
            set_motor_ramp(MOTOR_RIGHT, DEFAULT_RAMP);
            set_motor_ramp(MOTOR_LEFT, DEFAULT_RAMP);
            set_motor_speed(MOTOR_RIGHT, DEFAULT_SPEED-15);
            set_motor_speed(MOTOR_LEFT, DEFAULT_SPEED);
            checksensors(false);
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
            if(right_sensor_inside || left_sensor_inside) {
                mowstate = mowstate_running;
            }
            if(systimer_is_expired(&timeouttimer)) {
                mainstate = mainstate_stopped;
                stopreason = "Out of area";
            }
            break;
        case mowstate_tilted:
            if(tiltcount++ < 1) {
                mowstate = mowstate_backoff;
            } else {
                stopreason = "Mower tilted";
                mainstate = mainstate_stopped;
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
            systimer_start(&timeouttimer, TURN_TIMEOUT_MS);
            mowstate = mowstate_turn_2;
            break;
        case mowstate_turn_2:
            if(systimer_is_expired(&mowtimer)) {
                bool othersensor;
                if(turnleft) {
                    othersensor = right_sensor_inside;
                } else {
                    othersensor = left_sensor_inside;
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
            checksensors(false);
            if(systimer_is_expired(&mowtimer)) {
                if(right_sensor_inside == false && left_sensor_inside == false) {
                    mowstate = mowstate_out_of_area;
                } else {
                    if(findhome) {
                        mowstate = mowstate_refind_wire;
                        systimer_start(&timeouttimer, MAX_TIME_REFIND_WIRE);
                    } else {
                        mowstate = mowstate_running;
                    }
                }
            }
            break;
        case mowstate_start_after_charge:
            set_motor_ramp(MOTOR_RIGHT, DEFAULT_RAMP);
            set_motor_ramp(MOTOR_LEFT, DEFAULT_RAMP);
            set_motor_speed(MOTOR_RIGHT, -SLOW_SPEED);
            set_motor_speed(MOTOR_LEFT, -SLOW_SPEED);
            systimer_start(&mowtimer, REVERSE_AFTER_CHARGE_TIME_MS);
            mowstate = mowstate_start_after_charge_2;
            break;
        case mowstate_start_after_charge_2:
            if(systimer_is_expired(&mowtimer)) {
                turnleft = true;
                mowstate = mowstate_turn;
            }
            break;
        case mowstate_backoff:
            set_motor_ramp(MOTOR_RIGHT, IMMEDIATE_RAMP);
            set_motor_ramp(MOTOR_LEFT, IMMEDIATE_RAMP);
            set_motor_speed(MOTOR_RIGHT, 0);
            set_motor_speed(MOTOR_LEFT, 0);
            if(findhome) {
                systimer_start(&mowtimer, WAIT_FOR_CHARGE_DETECT);
            } else {
                systimer_start(&mowtimer, 0);
            }
            mowstate = mowstate_backoff_2;
            break;
        case mowstate_backoff_2:
            if(get_charger_connected()) {
                mainstate = mainstate_startcharge;
            }
            if(systimer_is_expired(&mowtimer)) { // Wait to see if charger is connected
                set_motor_ramp(MOTOR_RIGHT, FAST_RAMP);
                set_motor_ramp(MOTOR_LEFT, FAST_RAMP);
                set_motor_speed(MOTOR_RIGHT, -SLOW_SPEED);
                set_motor_speed(MOTOR_LEFT, -SLOW_SPEED);
                systimer_start(&mowtimer, BACKOFF_TIME_MS);
                mowstate = mowstate_backoff_3;
            }
            break;
        case mowstate_backoff_3:
            if(systimer_is_expired(&mowtimer)) {
                set_motor_speed(MOTOR_RIGHT, 0);
                set_motor_speed(MOTOR_LEFT, 0);
                turnleft = true;
                mowstate = mowstate_turn;
            }
            break;
        case mowstate_wire_found:
            systimer_start(&timeouttimer, TIME_BEFORE_MAX_TURN_IN_FIND_WIRE_MS);
            mowstate = mowstate_wire_found_slowturn;
            wire_found_count = 0;
            break;
        case mowstate_wire_found_slowturn:
            set_motor_ramp(MOTOR_RIGHT, DEFAULT_RAMP);
            set_motor_ramp(MOTOR_LEFT, DEFAULT_RAMP);
            if(right_sensor_inside == false && left_sensor_inside == false) {
                // turn left
                set_motor_speed(MOTOR_RIGHT, SLOW_SPEED);
                set_motor_speed(MOTOR_LEFT, 0);
                systimer_start(&timeouttimer, TURN_TIMEOUT_MS);
                mowstate = mowstate_wire_found_sharpturn;
            } else if(right_sensor_inside == false && left_sensor_inside == true) {
                // turn slight left
                set_motor_speed(MOTOR_RIGHT, INTERMEDIATE_SPEED+INTERMEDIATE_SPEED/4);
                set_motor_speed(MOTOR_LEFT, INTERMEDIATE_SPEED);
            } else if(right_sensor_inside == true && left_sensor_inside == false) {
                // turn right
                set_motor_ramp(MOTOR_RIGHT, SLOW_RAMP);
                set_motor_ramp(MOTOR_LEFT, SLOW_RAMP);
                set_motor_speed(MOTOR_RIGHT, 0);
                set_motor_speed(MOTOR_LEFT, INTERMEDIATE_SPEED);
            } else {
                // Both sensors inside, turn right
                set_motor_speed(MOTOR_RIGHT, 0);
                set_motor_speed(MOTOR_LEFT, SLOW_SPEED);
            }
            if(right_sensor_inside != last_right_sensor_inside || left_sensor_inside != last_left_sensor_inside) {
                systimer_start(&timeouttimer, TIME_BEFORE_MAX_TURN_IN_FIND_WIRE_MS);
            }
            if(systimer_is_expired(&timeouttimer)) {
                systimer_start(&timeouttimer, TURN_TIMEOUT_MS);
                mowstate = mowstate_wire_found_sharpturn;
            }
            checksensors(true);
            break;
        case mowstate_wire_found_sharpturn:
            set_motor_ramp(MOTOR_RIGHT, DEFAULT_RAMP);
            set_motor_ramp(MOTOR_LEFT, DEFAULT_RAMP);
            if(right_sensor_inside == false && left_sensor_inside == false) {
                // turn left
                set_motor_speed(MOTOR_RIGHT, SLOW_SPEED);
                set_motor_speed(MOTOR_LEFT, -SLOW_SPEED);
            } else if(right_sensor_inside == false && left_sensor_inside == true) {
                // turn slight left
                set_motor_speed(MOTOR_RIGHT, SLOW_SPEED+SLOW_SPEED/4);
                set_motor_speed(MOTOR_LEFT, SLOW_SPEED);
                mowstate = mowstate_wire_found_slowturn;
            } else if(right_sensor_inside == true && left_sensor_inside == false) {
                // turn right
                set_motor_speed(MOTOR_RIGHT, -SLOW_SPEED);
                set_motor_speed(MOTOR_LEFT, SLOW_SPEED);
                mowstate = mowstate_wire_found_turnright;
                systimer_start(&timeouttimer, TURN_TIMEOUT_MS);
            } else {
                // Both sensors inside, turn right
                set_motor_speed(MOTOR_RIGHT, -SLOW_SPEED);
                set_motor_speed(MOTOR_LEFT, SLOW_SPEED);
            }
            if(right_sensor_inside != last_right_sensor_inside || left_sensor_inside != last_left_sensor_inside) {
                systimer_start(&timeouttimer, TURN_TIMEOUT_MS);
            }           
            if(systimer_is_expired(&timeouttimer)) {
                if(right_sensor_inside==false && left_sensor_inside==false) {
                    mowstate = mowstate_running;
                } else {
                    mowstate = mowstate_running;
                }
            }
            checksensors(true);
            break;      
        case mowstate_wire_found_turnright:
            set_motor_ramp(MOTOR_RIGHT, DEFAULT_RAMP);
            set_motor_ramp(MOTOR_LEFT, DEFAULT_RAMP);
            // turn right
            set_motor_speed(MOTOR_RIGHT, -SLOW_SPEED);
            set_motor_speed(MOTOR_LEFT, SLOW_SPEED);

            systimer_start(&mowtimer, TURN_TIME_MS);
            systimer_start(&timeouttimer, TURN_TIMEOUT_MS);
            mowstate = mowstate_wire_found_turnright_2;

            checksensors(true);
            break;
        case mowstate_wire_found_turnright_2:
            if(systimer_is_expired(&mowtimer)) {
                if(right_sensor_inside == true) {
                    if(systimer_is_expired(&timeouttimer)) {
                        mowstate = mowstate_wire_found;
                    }
                } else {
                    set_motor_speed(MOTOR_RIGHT, 0);
                    set_motor_speed(MOTOR_LEFT, 0);
                    mowstate = mowstate_wire_found;
                }
            }
            break;


    }
    last_left_sensor_inside = left_sensor_inside;
    last_right_sensor_inside = right_sensor_inside;
}

void task_mowercontrol(void) {
    static keys_t lastpressedkey=0;
    static uint32_t chargecurrent;
    static systimer_t chargedata_timer, stoppedstate_timer;
    char tmpstr[20];
    uint32_t batteryvoltage;
    uint8_t soc;
    keys_t currentpressedkey;
    currentpressedkey = get_pressed_key();
    soc = get_battery_soc();

    if(soc <= POWER_OFF_SOC && get_charger_connected()==false) {
        if(systimer_is_expired(&lowsocpowerofftimer)) {
            poweroff();
        }
    } else {
        systimer_start(&lowsocpowerofftimer, TIME_IN_LOW_SOC_BEFORE_POWEROFF);
    }    

    switch(mainstate) {
        case mainstate_idle:
            mowing = false;
            stop_all_motors();
            print_init_menu();
            if(get_charger_connected()) {
                mainstate = mainstate_startcharge;
            }
            if(lastpressedkey == KEY_NONE) {
                if(currentpressedkey==KEY2) {
                    clear_display();
                    init_debugmenu();
                    mainstate = mainstate_debug;
                }
                if(currentpressedkey == KEYSTART) {
                    findhome = false;
                    mainstate = mainstate_mow;
                    mowstate = mowstate_startmow;
                }
                if(currentpressedkey == KEYHOME) {
                    findhome = true;
                    mainstate = mainstate_mow;
                    mowstate = mowstate_startmow;
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
        case mainstate_startcharge:
            stop_all_motors();
            set_charger_initiate(true);
            mainstate = mainstate_charging;
            chargecurrent = get_charge_current()*100;
            systimer_start(&chargedata_timer, 0);
            break;
        case mainstate_charging:
            stop_all_motors();
            // Simple average filter for charge current
            chargecurrent = (chargecurrent * 99) / 100;
            chargecurrent += get_charge_current();
            if(systimer_is_expired(&chargedata_timer)) {
                clear_display();
                print_text(0, "Charging");
                batteryvoltage = get_battery_voltage();
                sprintf(tmpstr, "%2ld.%1ldV %2d%%", batteryvoltage/1000, (batteryvoltage/100)%10, get_battery_soc());
                print_text(1, tmpstr);
                sprintf(tmpstr, "%4ldmA", chargecurrent/100);
                print_text(2, tmpstr);
                systimer_start(&chargedata_timer, 1000);
            }
            if(lastpressedkey==KEY_NONE && currentpressedkey==KEYBACK) {
                set_charger_initiate(false);
                mainstate = mainstate_idle;
            }
            if(get_charger_connected() == false) {
                set_charger_initiate(false);
                mainstate = mainstate_stopped;
                stopreason = "Charger disconnected";
            }
            if(get_charge_complete()) {
                set_charger_initiate(false);
                if(mowing) {
                    mainstate = mainstate_mow;
                    findhome = false;
                    mowstate = mowstate_start_after_charge;
                } else {
                    mainstate = mainstate_stopped;
                    stopreason = "Charge complete";
                }
            }
            break;
        case mainstate_stopped:
            stop_all_motors();
            clear_display();
            print_text(0, "Stopped");
            print_text(1, stopreason);
            print_text(2, "Press back");
            mowing = false;
            systimer_start(&stoppedstate_timer, TIME_IN_STOPPED_BEFORE_POWEROFF);
            mainstate = mainstate_stopped_2;
            break;
        case mainstate_stopped_2:
            if(systimer_is_expired(&stoppedstate_timer)) {
                poweroff();
            }
            if(lastpressedkey==KEY_NONE && currentpressedkey==KEYBACK) {
                mainstate = mainstate_idle;
            }
            break;
        default:
            DOASSERT();
            break;
    }
    lastpressedkey = currentpressedkey;

    sprintf(tmpstr, "State %d,%d", mainstate, mowstate);
    print_text(3, tmpstr);
}