#include "system.h"
#include <stdio.h>
#include <string.h>
#include "hal/hal_keyboard.h"
#include "hal/hal_sensors.h"
#include "hal/hal_motor.h"
#include "hal/hal_charger.h"
#include "hal/hal_adc.h"
#include "display.h"

#include "hal/hal_mcu.h"//debug

static bool charger_initiate = false;
static bool charger_charge = false;
static int8_t rightspeed=0, leftspeed=0, spindlespeed=0;
static bool wiresensor_mode_near = true, wiresensor_polarity = true;
static uint8_t key4_count = 0, key5_count = 0, key6_count = 0;
static bool menuactive;
typedef enum {
    taskstate_init = 0,
    taskstate_debugsensors,
    taskstate_debuggpio,
    taskstate_debugmotors,
    taskstate_debugcharger,
    taskstate_debugadc,
    taskstate_debugwiresensor,
    taskstate_inactive,

    taskstate_number_of_states
}taskstate_t;
static taskstate_t taskstate;

static keys_t lastpressedkey;

bool is_debugmenu_active(void){
    return menuactive;
}

void init_debugmenu(void) {
    menuactive = true;
    taskstate = taskstate_init;
    lastpressedkey = get_pressed_key();
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
    print_text(2, "3=MOTOR 4=CHARGER");
    print_text(3, "5=ADC 6=WIRESENS");
    sprintf(buffer, "%ld", systick_cnt);
    print_text(4, buffer);
}

static void print_sensors_menu(void)
{
    char buffer[64];

    sprintf(buffer, "L %s R %s", get_sensor(SENSOR_LEFT_WIRE_INSIDE) ? "IN " : "OUT", get_sensor(SENSOR_RIGHT_WIRE_INSIDE) ? "IN " : "OUT");
    print_text(0, buffer);
    sprintf(buffer, "%4s %5d", get_sensor(SENSOR_NEAR_WIRE) ? "NEAR" : "FAR", get_wiredistance());
    print_text(1, buffer);
    sprintf(buffer, "LIFT=%d FRONT=%d", (int)get_sensor(SENSOR_LIFT), (int)get_sensor(SENSOR_FRONT));
    print_text(2, buffer);
    sprintf(buffer, "pitch%4d roll%4d", get_pitch(), get_roll());
    print_text(3, buffer);
    if(get_sensor(SENSOR_STOPBTN)) {
        print_text(4, "KEYSTOP");
    } else {
        print_text(4, keyStrings[get_pressed_key()]);
    }
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

static void print_adc_menu(void)
{
    char buffer[64];
    int i;
    for(i=0;i<4;i++) {
        sprintf(buffer, "A%d 0x%03lX A%d 0x%03lX", i*2, hal_adc_get_value(i*2), i*2+1, hal_adc_get_value(i*2+1));
        print_text(i, buffer);
    }
}

static void print_motor_menu(void)
{
    char buffer[64];

    sprintf(buffer, "1 R %4d %u%u %ld", rightspeed, (uint8_t)((LPC_GPIO2->FIOPIN >> 5) & 1), (uint8_t)((LPC_GPIO2->FIOPIN >> 4) & 1), (long)get_motor_distance(MOTOR_RIGHT));
    print_text(0, buffer);
    
    sprintf(buffer, "2 L %4d %u%u %ld", leftspeed, (uint8_t)((LPC_GPIO2->FIOPIN >> 8) & 1), (uint8_t)((LPC_GPIO2->FIOPIN >> 9) & 1), (long)get_motor_distance(MOTOR_LEFT));
    print_text(1, buffer);
    
    sprintf(buffer, "3 S %4d %u%u %ld", spindlespeed, (uint8_t)((LPC_GPIO3->FIOPIN >> 25) & 1), (uint8_t)((LPC_GPIO2->FIOPIN >> 13) & 1), (long)get_motor_distance(MOTOR_SPINDLE));
    print_text(2, buffer);
}

static void print_charger_menu(void)
{
    char buffer[64];

    sprintf(buffer, "Connected=%d", get_charger_connected() ? 1 : 0);
    print_text(0, buffer);
    sprintf(buffer, "1 Initiate=%d", charger_initiate ? 1 : 0);
    print_text(1, buffer);
    sprintf(buffer, "2 Charge=%d", charger_charge ? 1 : 0);
    print_text(2, buffer);
}

#define RT(x) ((x > 0xfffffUL) ? (0xfffff) : x)
void print_wiresensor_menu(void)
{
    char buffer[64];
    sprintf(buffer, "1=%d 2=%d 3=TRIG", wiresensor_polarity ? 1 : 0, wiresensor_mode_near ? 1 : 0);
    print_text(0, buffer);
    if(debug_wire_idx >= NUMBER_OF_DEBUG_WIRE_TIMES) {
        sprintf(buffer, "%05lx %02x %05lx %02x", RT(debug_wire_times[0]), debug_wire_values[0], RT(debug_wire_times[1]), debug_wire_values[1]);
        print_text(1, buffer);
        sprintf(buffer, "%05lx %02x %05lx %02x", RT(debug_wire_times[2]), debug_wire_values[2], RT(debug_wire_times[3]), debug_wire_values[3]);
        print_text(2, buffer);
        sprintf(buffer, "%05lx %02x %05lx %02x", RT(debug_wire_times[4]), debug_wire_values[4], RT(debug_wire_times[5]), debug_wire_values[5]);
        print_text(3, buffer);
        sprintf(buffer, "%05lx %02x %05lx %02x", RT(debug_wire_times[6]), debug_wire_values[6], RT(debug_wire_times[7]), debug_wire_values[7]);
        print_text(4, buffer);
        sprintf(buffer, "%05lx %02x %05lx %02x", RT(debug_wire_times[8]), debug_wire_values[8], RT(debug_wire_times[9]), debug_wire_values[9]);
        print_text(5, buffer);
    }
}

void task_debugmenu(void) {
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
                if(currentpressedkey==KEY4) {
                    clear_display();
                    taskstate = taskstate_debugcharger;
                }
                if(currentpressedkey==KEY5) {
                    clear_display();
                    taskstate = taskstate_debugadc;
                }
                if(currentpressedkey==KEY6) {
                    clear_display();
                    taskstate = taskstate_debugwiresensor;
                    wire_sensor_debug(true, wiresensor_polarity, wiresensor_mode_near, true);
                    set_text_size(10);
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
        case taskstate_debugadc:
            print_adc_menu();
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
                    rightspeed = (rightspeed >= 100) ? -100 : rightspeed + 1;
                    set_motor_speed(MOTOR_RIGHT, rightspeed);
                }
                if(currentpressedkey==KEY2) {
                    leftspeed = (leftspeed >= 100) ? -100 : leftspeed + 1;
                    set_motor_speed(MOTOR_LEFT, leftspeed);
                }
                if(currentpressedkey==KEY3) {
                    spindlespeed = (spindlespeed >= 100) ? -100 : spindlespeed + 1;
                    set_motor_speed(MOTOR_SPINDLE, spindlespeed);
                }
                if(currentpressedkey==KEY4) {
                    if((LPC_GPIO2->FIOPIN >> 5) & 1) {
                        LPC_GPIO2->FIOCLR = (1 << 5);
                    } else {
                        LPC_GPIO2->FIOSET = (1 << 5);
                    }
                    key4_count++;
                    if(key4_count >= 2) {
                        if((LPC_GPIO2->FIOPIN >> 4) & 1) {
                            LPC_GPIO2->FIOCLR = (1 << 4);
                        } else {
                            LPC_GPIO2->FIOSET = (1 << 4);
                        }
                        key4_count = 0;
                    }
                }
                if(currentpressedkey==KEY5) {
                    if((LPC_GPIO2->FIOPIN >> 8) & 1) {
                        LPC_GPIO2->FIOCLR = (1 << 8);
                    } else {
                        LPC_GPIO2->FIOSET = (1 << 8);
                    }
                    key5_count++;
                    if(key5_count >= 2) {
                        if((LPC_GPIO2->FIOPIN >> 9) & 1) {
                            LPC_GPIO2->FIOCLR = (1 << 9);
                        } else {
                            LPC_GPIO2->FIOSET = (1 << 9);
                        }
                        key5_count = 0;
                    }
                }
                if(currentpressedkey==KEY6) {
                    if((LPC_GPIO3->FIOPIN >> 25) & 1) {
                        LPC_GPIO3->FIOCLR = (1 << 25);
                    } else {
                        LPC_GPIO3->FIOSET = (1 << 25);
                    }
                    key6_count++;
                    if(key6_count >= 2) {
                        if((LPC_GPIO2->FIOPIN >> 13) & 1) {
                            LPC_GPIO2->FIOCLR = (1 << 13);
                        } else {
                            LPC_GPIO2->FIOSET = (1 << 13);
                        }
                        key6_count = 0;
                    }
                }
                if(currentpressedkey==KEY7) {
                    rightspeed = (rightspeed <= -100) ? 100 : rightspeed - 1;
                    set_motor_speed(MOTOR_RIGHT, rightspeed);
                }
                if(currentpressedkey==KEY8) {
                    leftspeed = (leftspeed <= -100) ? 100 : leftspeed - 1;
                    set_motor_speed(MOTOR_LEFT, leftspeed);
                }
                if(currentpressedkey==KEY9) {
                    spindlespeed = (spindlespeed <= -100) ? 100 : spindlespeed - 1;
                    set_motor_speed(MOTOR_SPINDLE, spindlespeed);
                }
            }
            break;
        case taskstate_debugcharger:
            print_charger_menu();
            set_charger_initiate(charger_initiate);
            set_charger_active(charger_charge);
            if(lastpressedkey==KEY_NONE) {
                if(currentpressedkey==KEYBACK) {
                    clear_display();
                    taskstate = taskstate_init;
                }
                if(currentpressedkey==KEY1) {
                    charger_initiate = !charger_initiate;
                }
                if(currentpressedkey==KEY2) {
                    charger_charge = !charger_charge;
                }
            }
            break;     
        case taskstate_debugwiresensor:
            print_wiresensor_menu();
            if(lastpressedkey==KEY_NONE) {
                if(currentpressedkey==KEYBACK) {
                    clear_display();
                    wire_sensor_debug(false, wiresensor_polarity, wiresensor_mode_near, false);
                    set_text_size(13);
                    taskstate = taskstate_init;
                }
                if(currentpressedkey==KEY1) {
                    wiresensor_polarity = !wiresensor_polarity;
                    wire_sensor_debug(true, wiresensor_polarity, wiresensor_mode_near, false);
                }
                if(currentpressedkey==KEY2) {
                    wiresensor_mode_near = !wiresensor_mode_near;
                    wire_sensor_debug(true, wiresensor_polarity, wiresensor_mode_near, false);
                }
                if(currentpressedkey==KEY3) {
                    wire_sensor_debug(true, wiresensor_polarity, wiresensor_mode_near, true);
                    clear_display();
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