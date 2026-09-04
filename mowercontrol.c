#include "system.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mowercontrol.h"
#include "hal/hal_keyboard.h"
#include "hal/hal_sensors.h"
#include "hal/hal_motor.h"
#include "hal/hal_charger.h"
#include "hal/hal_power.h"
#include "hal/hal_rtc.h"
#include "display.h"
#include "debugmenu.h"
#include "menu.h"
#include "scheduler.h"
#include "remotecom.h"

#define NUMBER_OF_STOPREASONS 8
static const char *stopreason_text[NUMBER_OF_STOPREASONS] = {
    "power on",
    "Stopbtn pressed",
    "Out of area",
    "Mower tilted",
    "Timeout in turn",
    "Charger disconnected",
    "Charge complete",
    "Runtime timeout"
};

uint8_t stopreason;
bool is_stopped = false;
static bool mowing = false;
bool avoid_downhill = false;
bool sideways_down = false;
uint8_t circlespeed = 50;
static bool remote_control_enabled = false;
static int8_t remote_control_left_speed;
static int8_t remote_control_right_speed;
static int8_t remote_control_disc_speed;

typedef enum {
    mainstate_idle = 0,
    mainstate_debug,
    mainstate_menu,
    mainstate_mow,
    mainstate_startcharge,
    mainstate_charging,
    mainstate_wait_for_schedule,
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
    mowstate_tilted,
    mowstate_sideways_downhill,
    mowstate_obstacle_timeout,
    mowstate_rc_idle,
    mowstate_rc_running,
    mowstate_rc_stopped,
    mowstate_rc_forcerun,
    mowstate_rc_turn,
    mowstate_rc_turn_forcerun
}mowstate_t;

static mowstate_t mowstate;
static bool turnleft; // Indicates turn direction if turning. true = turn left, false = turn right
static bool findhome;
static bool circlecut;
static systimer_t lowsocpowerofftimer;
static systimer_t circlecuttimer;
static systimer_t obstacletimer;
static systimer_t forcerunwiretimer;
static systimer_t rc_turn_timer;
static uint8_t tiltcount;
static uint8_t timeoutcount;
static uint8_t backoffcount;

#define SLOW_SPEED      20
#define INTERMEDIATE_SPEED 30
#define DEFAULT_SPEED   45
#define SPINDLE_DEFAULT_SPEED 80
#define SLOW_RAMP       10
#define DEFAULT_RAMP    20
#define FAST_RAMP       30
#define IMMEDIATE_RAMP 100
#define TURN_TIME_MS    2500
#define TURN_TIME_VARIATION_MS 500
#define TURN_TIMEOUT_MS 10000
#define BACKOFF_TIME_MS 1200
#define WAIT_FOR_CHARGE_DETECT 4000
#define MAX_TIME_OUT_OF_AREA 4000
#define MAX_TIME_RC_OUTSIDE_WIRE 10000
#define RC_TURN_STOPPED_MS_PER_DEGREE 28
#define RC_TURN_INNER_PERCENT 50 // Inner wheel speed in percent of outer wheel during moving turn
#define RC_TURN_MOVING_MS_PER_DEGREE_X_DIFF 840 // Measured. Lower than for a pivot turn since a moving turn slips less
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
#define CIRCLECUT_SPEED_INCREASE_INTERVAL_MS 5000UL
#define CIRCLECUT_START_SPEED 20
#define MAX_RUNTIME_WITHOUT_HITTING_OBSTACLE 180000UL

void init_mowercontrol(bool start_stopped, uint8_t reason_code) {
    stopreason = reason_code;
    mainstate = start_stopped ? mainstate_stopped : mainstate_idle;
    findhome = false;
    circlecut = false;
    remote_control_enabled = false;
    systimer_start(&lowsocpowerofftimer, TIME_IN_LOW_SOC_BEFORE_POWEROFF);
}

uint8_t mowercontrol_get_state(void) {
    if(remote_control_enabled) {
        switch(mainstate) {
            case mainstate_mow:
                if(mowstate == mowstate_rc_turn || mowstate == mowstate_rc_turn_forcerun) {
                    return 34; // rc_turning
                } else {
                    return 33; // rc_running
                }
            case mainstate_startcharge:
            case mainstate_charging:
                return 37; // rc_charging
            //todo: 35 rc_normal_mow, once REMOTE_CONTROL_MOW is implemented
            //todo: 36 rc_finding_charge, once REMOTE_CONTROL_FIND_CHARGER is implemented
            default:
                return 32; // rc_stopped
        }
    } else {
        switch(mainstate) {
            case mainstate_mow: 
                return 1; // mowing
            case mainstate_startcharge:
            case mainstate_charging: 
                return 2; // charging
            case mainstate_wait_for_schedule: 
                return 3;
            case mainstate_stopped:
            case mainstate_stopped_2: 
                return 4; // stopped
            default: 
                return 0; // idle
        }
    }
}

bool remotecontrol_run(bool forcerun, int8_t left_speed, int8_t right_speed, int8_t disc_speed) {
    if (!remote_control_enabled) {
        return false;
    }
    if(mainstate == mainstate_mow) { // todo: handle charge, stopped, etc
        if(forcerun) {
            if(mowstate != mowstate_rc_forcerun) {
                systimer_start(&forcerunwiretimer, MAX_TIME_RC_OUTSIDE_WIRE);
            }
            mowstate = mowstate_rc_forcerun;
        } else if(mowstate == mowstate_rc_idle || mowstate == mowstate_rc_running) {
            mowstate = mowstate_rc_running;
        } else {
            return false;
        }
        remote_control_left_speed = left_speed;
        remote_control_right_speed = right_speed;
        remote_control_disc_speed = disc_speed;
        return true;
    } else {
        return false;
    }
}

bool remotecontrol_turn(bool forcerun, int8_t wheel_speed, uint8_t turn_angle, bool turn_right, int8_t disc_speed) {
    int8_t left_speed, right_speed, inner;
    uint32_t duration_ms;

    if (!remote_control_enabled) {
        return false;
    }
    if (mainstate == mainstate_mow) {
        if(forcerun) {
            if(mowstate != mowstate_rc_turn_forcerun) {
                systimer_start(&forcerunwiretimer, MAX_TIME_RC_OUTSIDE_WIRE);
            }
            mowstate = mowstate_rc_turn_forcerun;
        } else if(mowstate == mowstate_rc_idle || mowstate == mowstate_rc_running) {
            mowstate = mowstate_rc_turn;
            
        } else {
            return false;
        }

        if (wheel_speed == 0) {
            left_speed = turn_right ? SLOW_SPEED : -SLOW_SPEED;
            right_speed = turn_right ? -SLOW_SPEED : SLOW_SPEED;
            duration_ms = (uint32_t)turn_angle * RC_TURN_STOPPED_MS_PER_DEGREE;
        } else {
            inner = (int8_t)((int16_t)wheel_speed * RC_TURN_INNER_PERCENT / 100);
            left_speed = turn_right ? wheel_speed : inner;
            right_speed = turn_right ? inner : wheel_speed;
            duration_ms = (uint32_t)turn_angle * RC_TURN_MOVING_MS_PER_DEGREE_X_DIFF / (uint32_t)abs(left_speed - right_speed);
        }

        set_motor_ramp(MOTOR_RIGHT, DEFAULT_RAMP);
        set_motor_ramp(MOTOR_LEFT, DEFAULT_RAMP);
        set_motor_ramp(MOTOR_SPINDLE, DEFAULT_RAMP);
        set_motor_speed(MOTOR_RIGHT, right_speed);
        set_motor_speed(MOTOR_LEFT, left_speed);

        remote_control_left_speed = wheel_speed;
        remote_control_right_speed = wheel_speed;
        remote_control_disc_speed = disc_speed;
        systimer_start(&rc_turn_timer, duration_ms);
        return true;
    } else {
        return false;
    }
}

static void print_init_menu(void)
{
    char buffer[64];
    uint32_t batteryvoltage;
    batteryvoltage = get_battery_voltage();
    sprintf(buffer, "%2ld.%1ldV %2d%% %02d:%02d", batteryvoltage/1000, (batteryvoltage/100)%10, get_battery_soc(), get_rtc_hour(), get_rtc_minute());
    print_text(0, buffer);
    if(remote_control_enabled) {
        print_text(1, "Remote controlled");
        print_text(2, "");
    } else {
        print_text(1, "START=mow OK=Settin");
        print_text(2, "2=Dbg 3=Circle 4=RC");
    }
}

void stop_all_motors(void) {
    set_motor_ramp(MOTOR_RIGHT, IMMEDIATE_RAMP);
    set_motor_ramp(MOTOR_LEFT, IMMEDIATE_RAMP);
    set_motor_ramp(MOTOR_SPINDLE, IMMEDIATE_RAMP);
    set_motor_speed(MOTOR_RIGHT, 0);
    set_motor_speed(MOTOR_LEFT, 0);
    set_motor_speed(MOTOR_SPINDLE, 0);
}

static bool mowertilted(void) {
    return(abs(get_pitch()) > TILTED_ANGLE || abs(get_roll()) > TILTED_ANGLE);
}

/* Check sensors and update mainstate, mowstate and turnleft if needed */
void checksensors(bool wirefound) {
    if(get_charger_connected()) {
        mainstate = mainstate_startcharge;
    } else if(mowertilted()) {
        stop_all_motors();
        mowstate = mowstate_tilted;
        circlecut = false;
    } else if(get_sensor(SENSOR_LIFT)) {
        set_motor_ramp(MOTOR_SPINDLE, IMMEDIATE_RAMP);
        set_motor_speed(MOTOR_SPINDLE, 0);
        mowstate = mowstate_backoff;
        circlecut = false;
        systimer_start(&obstacletimer, MAX_RUNTIME_WITHOUT_HITTING_OBSTACLE);
        timeoutcount = 0;
    } else if(get_sensor(SENSOR_FRONT)) {
        mowstate = mowstate_backoff;
        circlecut = false;
        systimer_start(&obstacletimer, MAX_RUNTIME_WITHOUT_HITTING_OBSTACLE);
        timeoutcount = 0;
    } else if(get_sensor(SENSOR_LEFT_WIRE_INSIDE) == false) {
        circlecut = false;
        if(findhome) {
            if(!wirefound) {
                mowstate = mowstate_wire_found;
            }
        } else {
            turnleft = false;
            mowstate = mowstate_turn;
        }
        systimer_start(&obstacletimer, MAX_RUNTIME_WITHOUT_HITTING_OBSTACLE);
        timeoutcount = 0;
    } else if(get_sensor(SENSOR_RIGHT_WIRE_INSIDE) == false) {
        circlecut = false;
        if(findhome) {
            if(!wirefound) {
                mowstate = mowstate_wire_found;
            }
        } else {
            turnleft = true;
            mowstate = mowstate_turn;
        }
        systimer_start(&obstacletimer, MAX_RUNTIME_WITHOUT_HITTING_OBSTACLE);
        timeoutcount = 0;
    } else if(circlecut) {
        systimer_start(&obstacletimer, MAX_RUNTIME_WITHOUT_HITTING_OBSTACLE);
    } else if(systimer_is_expired(&obstacletimer)) {
        mowstate = mowstate_obstacle_timeout;
    }
}

/* Mow control state machine. Active when mainstate is mainstate_mow */
void mow_state(void) {
    static systimer_t mowtimer, timeouttimer;
    int roll, pitch;
    char tmpstr[20];
    uint32_t batteryvoltage;
    uint32_t progress, curve;
    static uint32_t circlecut_speed;
    uint8_t soc;
    static bool last_left_sensor_inside, last_right_sensor_inside;
    bool left_sensor_inside, right_sensor_inside;
    bool othersensor;
    static bool downhill_turnleft;
    batteryvoltage = get_battery_voltage();
    soc = get_battery_soc();

    clear_display();
    if(findhome) {
        print_text(0, "Finding home...");
    } else if(circlecut) {
        print_text(0, "Circle cutting...");
    } else if(remote_control_enabled){
        print_text(0, "Remote controlled");
    } else {
        print_text(0, "Mowing...");
    }

    sprintf(tmpstr, "%2ld.%1ldV %2d%%", batteryvoltage/1000, (batteryvoltage/100)%10, soc);
    print_text(1, tmpstr);

    if(get_sensor(SENSOR_STOPBTN)) {
        mainstate = mainstate_stopped;
        remote_control_enabled = false;
        stopreason = 1;
        return;
    }
    left_sensor_inside = get_sensor(SENSOR_LEFT_WIRE_INSIDE);
    right_sensor_inside = get_sensor(SENSOR_RIGHT_WIRE_INSIDE);

    // Make sure disc stops in all states when tilted
    if(mowertilted()) {
        set_motor_speed(MOTOR_SPINDLE, 0);
    }

    switch(mowstate) {
        case mowstate_startmow:
            if(right_sensor_inside == false || left_sensor_inside == false) {
                mainstate = mainstate_stopped;
                stopreason = 2;
            } else {
                mowstate = mowstate_running;
                mowing = true;
            }
            backoffcount = 0;
            if(circlecut) {
                systimer_start(&circlecuttimer, CIRCLECUT_SPEED_INCREASE_INTERVAL_MS);
                circlecut_speed = CIRCLECUT_START_SPEED * 1000u;
            }
            systimer_start(&obstacletimer, MAX_RUNTIME_WITHOUT_HITTING_OBSTACLE);
            break;
        case mowstate_running:
            tiltcount = 0;
            set_motor_ramp(MOTOR_RIGHT, DEFAULT_RAMP);
            set_motor_ramp(MOTOR_LEFT, DEFAULT_RAMP);
            set_motor_ramp(MOTOR_SPINDLE, DEFAULT_RAMP);
            set_motor_speed(MOTOR_RIGHT, DEFAULT_SPEED);
            
            // Handle circle cutting mode 
            if(circlecut) {
                if(systimer_is_expired(&circlecuttimer)) {
                    systimer_start(&circlecuttimer, CIRCLECUT_SPEED_INCREASE_INTERVAL_MS);
                    progress = ((circlecut_speed/1000UL - CIRCLECUT_START_SPEED ) * 100) / (DEFAULT_SPEED - CIRCLECUT_START_SPEED );
                    curve = ((uint32_t)(400u - 3u*progress) * (uint32_t)circlespeed);
                    circlecut_speed += (uint32_t)curve / 100u;
                    if(circlecut_speed >= DEFAULT_SPEED * 1000u) {
                        circlecut = false;
                    }
                }
                set_motor_speed(MOTOR_LEFT, circlecut_speed / 1000u);
            } else {
                set_motor_speed(MOTOR_LEFT, DEFAULT_SPEED);
            }
            
            // Handle downhill problem workarounds
            roll = get_roll();
            pitch = get_pitch();
            if(avoid_downhill && pitch < -9) {
                mowstate = mowstate_running_downhill;
                if(roll > 0) {
                    downhill_turnleft = true;
                } else {
                    downhill_turnleft = false;
                }
            } else if(sideways_down && pitch < -9) {
                mowstate = mowstate_sideways_downhill;
                if(roll > 0) {
                    downhill_turnleft = true;
                } else {
                    downhill_turnleft = false;
                }
            }

            // Save battery so we can get to changer before running out of battery
            if(soc < TURN_OFF_DISC_SOC || mowertilted()) {
                set_motor_speed(MOTOR_SPINDLE, 0);
            } else {
                set_motor_speed(MOTOR_SPINDLE, SPINDLE_DEFAULT_SPEED);
            }

            // Check if any sensor indicates a problem
            checksensors(false);

            // Go to charger if its time to recharge battery
            if(soc < GO_TO_CHARGE_STATION_SOC || !in_schedule_time()) {
                findhome = true;
            }
            break;
        case mowstate_running_downhill:
            tiltcount = 0;
            set_motor_ramp(MOTOR_RIGHT, DEFAULT_RAMP);
            set_motor_ramp(MOTOR_LEFT, DEFAULT_RAMP);
            // If mower is going downhill then turn around to avoid getting stuck at end of slope
            roll = get_roll();
            pitch = get_pitch();
            if(pitch > 0) {
                mowstate = mowstate_running;
            } else {
                // Change turn direction?
                if(roll > 9) {
                    downhill_turnleft = true;
                } else if(roll <-9) {
                    downhill_turnleft = false;
                }
                if(downhill_turnleft) {
                    set_motor_speed(MOTOR_RIGHT, INTERMEDIATE_SPEED);
                    set_motor_speed(MOTOR_LEFT, -INTERMEDIATE_SPEED/2);
                } else {
                    set_motor_speed(MOTOR_RIGHT, -INTERMEDIATE_SPEED/2);
                    set_motor_speed(MOTOR_LEFT, INTERMEDIATE_SPEED);
                }
            }
            checksensors(false);
            break;
        case mowstate_sideways_downhill:
            tiltcount = 0;
            set_motor_ramp(MOTOR_RIGHT, DEFAULT_RAMP);
            set_motor_ramp(MOTOR_LEFT, DEFAULT_RAMP);
            // If mower is going downhill then turn around to avoid getting stuck at end of slope
            roll = get_roll();
            pitch = get_pitch();
            if(pitch > 0) {
                mowstate = mowstate_running;
            } else {
                // Change turn direction?
                if(roll > 9) {
                    downhill_turnleft = true;
                } else if(roll <-9) {
                    downhill_turnleft = false;
                }
                if(downhill_turnleft) {
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
                stopreason = 2;
            }
            break;
        case mowstate_tilted:
            if(tiltcount++ < 1) {
                mowstate = mowstate_backoff;
            } else {
                stopreason = 3;
                mainstate = mainstate_stopped;
            }
            break;
        case mowstate_obstacle_timeout:
            if(timeoutcount < 2) {
                timeoutcount++;
                systimer_start(&obstacletimer, MAX_RUNTIME_WITHOUT_HITTING_OBSTACLE);
                mowstate = mowstate_backoff;
            } else {
                stopreason = 7;
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
            systimer_start(&mowtimer, TURN_TIME_MS + systick_cnt % TURN_TIME_VARIATION_MS);
            systimer_start(&timeouttimer, TURN_TIMEOUT_MS);
            mowstate = mowstate_turn_2;
            break;
        case mowstate_turn_2:
            if(mowertilted()) {
                stop_all_motors();
                mowstate = mowstate_tilted;
                circlecut = false;
            } else if(get_sensor(SENSOR_FRONT) || get_sensor(SENSOR_LIFT)) {
                set_motor_ramp(MOTOR_SPINDLE, IMMEDIATE_RAMP);
                set_motor_speed(MOTOR_SPINDLE, 0);
                if(backoffcount >= 5) {
                    stopreason = 4;
                    mainstate = mainstate_stopped;
                } else {
                    backoffcount++;
                    mowstate = mowstate_backoff;
                }
            } else if(systimer_is_expired(&mowtimer)) {
                if(turnleft) {
                    othersensor = right_sensor_inside;
                } else {
                    othersensor = left_sensor_inside;
                }
                if(othersensor == false) {
                    if(systimer_is_expired(&timeouttimer)) {
                        if(backoffcount >= 5) {
                            stopreason = 4;
                            mainstate = mainstate_stopped;
                        } else {
                            backoffcount++;
                            mowstate = mowstate_backoff;
                        }
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
                backoffcount = 0;
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
            systimer_start(&obstacletimer, MAX_RUNTIME_WITHOUT_HITTING_OBSTACLE);
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
                turnleft = (systick_cnt & 1) != 0;
                mowstate = mowstate_turn;
            }
            break;
        case mowstate_wire_found:
            systimer_start(&timeouttimer, TIME_BEFORE_MAX_TURN_IN_FIND_WIRE_MS);
            mowstate = mowstate_wire_found_slowturn;
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
        case mowstate_rc_idle:
            stop_all_motors();
            break;
        case mowstate_rc_running:
            set_motor_ramp(MOTOR_RIGHT, DEFAULT_RAMP);
            set_motor_ramp(MOTOR_LEFT, DEFAULT_RAMP);
            set_motor_ramp(MOTOR_SPINDLE, DEFAULT_RAMP);
            set_motor_speed(MOTOR_RIGHT, remote_control_right_speed);
            set_motor_speed(MOTOR_LEFT, remote_control_left_speed);
            if(mowertilted() || get_sensor(SENSOR_LIFT)) {
                set_motor_ramp(MOTOR_SPINDLE, IMMEDIATE_RAMP);
                set_motor_speed(MOTOR_SPINDLE, 0);
                mowstate = mowstate_rc_stopped;
                break;
            } else {
                set_motor_speed(MOTOR_SPINDLE, remote_control_disc_speed);
            }
            if(get_sensor(SENSOR_FRONT)) {
                mowstate = mowstate_rc_stopped;
            } else if(get_sensor(SENSOR_LEFT_WIRE_INSIDE) == false) {
                mowstate = mowstate_rc_stopped;
            } else if(get_sensor(SENSOR_RIGHT_WIRE_INSIDE) == false) {
                mowstate = mowstate_rc_stopped;
            }
            break;
        case mowstate_rc_stopped:
            stop_all_motors();
            mowstate = mowstate_rc_idle;
            break;
        case mowstate_rc_forcerun:
            set_motor_ramp(MOTOR_RIGHT, DEFAULT_RAMP);
            set_motor_ramp(MOTOR_LEFT, DEFAULT_RAMP);
            set_motor_ramp(MOTOR_SPINDLE, DEFAULT_RAMP);
            set_motor_speed(MOTOR_RIGHT, remote_control_right_speed);
            set_motor_speed(MOTOR_LEFT, remote_control_left_speed);
            if(mowertilted() || get_sensor(SENSOR_LIFT)) {
                set_motor_ramp(MOTOR_SPINDLE, IMMEDIATE_RAMP);
                set_motor_speed(MOTOR_SPINDLE, 0);
                break;
            } else {
                set_motor_speed(MOTOR_SPINDLE, remote_control_disc_speed);
            }
            if(get_sensor(SENSOR_LEFT_WIRE_INSIDE) || get_sensor(SENSOR_RIGHT_WIRE_INSIDE)) {
                systimer_start(&forcerunwiretimer, MAX_TIME_RC_OUTSIDE_WIRE);
            }
            if(systimer_is_expired(&forcerunwiretimer)) {
                mainstate = mainstate_stopped;
                remote_control_enabled = false;
                stopreason = 2; // Out of area
            }
            break;
        case mowstate_rc_turn:
            if(mowertilted() || get_sensor(SENSOR_LIFT)) {
                set_motor_ramp(MOTOR_SPINDLE, IMMEDIATE_RAMP);
                set_motor_speed(MOTOR_SPINDLE, 0);
                mowstate = mowstate_rc_stopped;
                break;
            } else {
                set_motor_speed(MOTOR_SPINDLE, remote_control_disc_speed);
            }
            if(systimer_is_expired(&rc_turn_timer)) {
                mowstate = mowstate_rc_running;
            }
            if(get_sensor(SENSOR_FRONT)) {
                mowstate = mowstate_rc_stopped;
            } else if(get_sensor(SENSOR_LEFT_WIRE_INSIDE) == false) {
                mowstate = mowstate_rc_stopped;
            } else if(get_sensor(SENSOR_RIGHT_WIRE_INSIDE) == false) {
                mowstate = mowstate_rc_stopped;
            }
            break;
        case mowstate_rc_turn_forcerun:
            if(mowertilted() || get_sensor(SENSOR_LIFT)) {
                set_motor_ramp(MOTOR_SPINDLE, IMMEDIATE_RAMP);
                set_motor_speed(MOTOR_SPINDLE, 0);
                break;
            } else {
                set_motor_speed(MOTOR_SPINDLE, remote_control_disc_speed);
            }
            if(get_sensor(SENSOR_LEFT_WIRE_INSIDE) || get_sensor(SENSOR_RIGHT_WIRE_INSIDE)) {
                systimer_start(&forcerunwiretimer, MAX_TIME_RC_OUTSIDE_WIRE);
            }
            if(systimer_is_expired(&forcerunwiretimer)) {
                mainstate = mainstate_stopped;
                remote_control_enabled = false;
                stopreason = 2; // Out of area
                break;
            }
            if(systimer_is_expired(&rc_turn_timer)) {
                mowstate = mowstate_rc_running;
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
    static uint8_t lastreportedstate = 0xFF;
    char tmpstr[20];
    uint32_t batteryvoltage;
    uint8_t soc;
    uint8_t reportedstate;
    keys_t currentpressedkey;
    currentpressedkey = get_pressed_key();
    soc = get_battery_soc();

    if(soc <= POWER_OFF_SOC && get_charger_connected()==false) {
        if(systimer_is_expired(&lowsocpowerofftimer)) {
            store_settings();
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
                if(currentpressedkey == KEY2) {
                    clear_display();
                    init_debugmenu();
                    mainstate = mainstate_debug;
                }
                if(currentpressedkey == KEY3) {
                    clear_display();
                    findhome = false;
                    circlecut = true;
                    mainstate = mainstate_mow;
                    mowstate = mowstate_startmow;
                }
                if(currentpressedkey == KEY4) {
                    clear_display();
                    findhome = false;
                    circlecut = false;
                    remote_control_enabled = true;
                    mainstate = mainstate_mow;
                    mowstate = mowstate_rc_idle;
                    backoffcount = 0;
                    systimer_start(&obstacletimer, MAX_RUNTIME_WITHOUT_HITTING_OBSTACLE);
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
                if(currentpressedkey == KEYOK) {
                    clear_display();
                    init_menu();
                    mainstate = mainstate_menu;
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
        case mainstate_menu:
            if(is_menu_active()) {
                task_menu();
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
                stopreason = 5;
            }
            if(get_charge_complete()) {
                set_charger_initiate(false);
                if(mowing) {
                    mainstate = mainstate_wait_for_schedule;
                } else {
                    mainstate = mainstate_stopped;
                    stopreason = 6;
                }
            }
            break;
        case mainstate_wait_for_schedule:
            clear_display();
            print_text(0, "Wait for schedule");
            if(lastpressedkey==KEY_NONE && currentpressedkey==KEYBACK) {
                mainstate = mainstate_idle;
            }
            if(in_schedule_time()) {
                mainstate = mainstate_mow;
                findhome = false;
                mowstate = mowstate_start_after_charge;
            }
            break;
        case mainstate_stopped:
            stop_all_motors();
            clear_display();
            print_text(0, "Stopped");
            if(stopreason < NUMBER_OF_STOPREASONS) {
                print_text(1, stopreason_text[stopreason]);
            }
            print_text(2, "Press back");
            mowing = false;
            systimer_start(&stoppedstate_timer, TIME_IN_STOPPED_BEFORE_POWEROFF);
            mainstate = mainstate_stopped_2;
            break;
        case mainstate_stopped_2:
            if(systimer_is_expired(&stoppedstate_timer)) {
                store_settings();
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
    is_stopped = (mainstate == mainstate_stopped || mainstate == mainstate_stopped_2);

    reportedstate = mowercontrol_get_state();
    if(reportedstate != lastreportedstate) {
        lastreportedstate = reportedstate;
        remotecom_send_status();
    }

    //sprintf(tmpstr, "State %d,%d", mainstate, mowstate);
    //print_text(3, tmpstr);
}