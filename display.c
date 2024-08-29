#include "system.h"
#include <stdio.h>
#include <string.h>
#include "hal/hal_display.h"
#include "hal/hal_keyboard.h"

#define FONT_HEIGHT 13
#define FONT_WIDTH 7

void init_display(void) {
    u8g_FirstPage(&u8g);
    do {
        u8g_SetDefaultBackgroundColor(&u8g);
        u8g_DrawBox(&u8g, 0, 0, 128, 64);
        u8g_SetDefaultForegroundColor(&u8g);
        u8g_SetFont(&u8g, u8g_font_7x13B);
    } while ( u8g_NextPage(&u8g) );
}

char *keyStrings[KEY_NUMBER_OF_KEYS] = {
    "KEY_NONE", "KEY8", "KEY9", "KEY0", "KEYSTART" ,
    "KEY6",     "KEYOK", "KEYDOWN", "KEY7" ,
    "KEYBACK",  "KEYUP", "KEY4",    "KEY5" ,
    "KEYHOME",  "KEY1",  "KEY2",    "KEY3"
};

void task_display(void) {
    char buffer[64];
    u8g_FirstPage(&u8g);
    do {
        u8g_DrawStr(&u8g,  0, FONT_HEIGHT, "systick_cnt");
        sprintf(buffer, "%ld", systick_cnt);
        u8g_DrawStr(&u8g,  0, FONT_HEIGHT*2, buffer);

        sprintf(buffer, "key 0x%08lX", getKeyboardDebugvalue());
        u8g_DrawStr(&u8g,  0, FONT_HEIGHT*3, buffer);

        //sprintf(buffer, "key %d", get_pressed_key());
        //u8g_DrawStr(&u8g,  0, FONT_HEIGHT*4, buffer);
        u8g_DrawStr(&u8g,  0, FONT_HEIGHT*4, keyStrings[get_pressed_key()]);

    } while ( u8g_NextPage(&u8g) );
}
