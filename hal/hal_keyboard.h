#ifndef _HAL_KEYBOARD_H
#define _HAL_KEYBOARD_H
#include <stdbool.h>

typedef enum {
    KEY_NONE = 0, KEY8,     KEY9,  KEY0,    KEYSTART ,
    KEY6,     KEYOK, KEYDOWN, KEY7 ,
    KEYBACK,  KEYUP, KEY4,    KEY5 ,
    KEYHOME,  KEY1,  KEY2,    KEY3, KEY_NUMBER_OF_KEYS
} keys_t;

void init_hal_keyboard(void);
bool get_power_button(void);
bool get_stop_button(void);
keys_t get_pressed_key(void);
void task_keyboard(void);

uint32_t getKeyboardDebugvalue(void);

 
#endif