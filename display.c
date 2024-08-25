#include "system.h"
#include <stdio.h>
#include <string.h>
#include "hal/hal_display.h"

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

void task_display(void) {
    char buffer[64];
    u8g_FirstPage(&u8g);
    do {
        u8g_DrawStr(&u8g,  0, FONT_HEIGHT, "systick_cnt");
        sprintf(buffer, "%ld", systick_cnt);
        u8g_DrawStr(&u8g,  0, FONT_HEIGHT*2, buffer);
    } while ( u8g_NextPage(&u8g) );
}
