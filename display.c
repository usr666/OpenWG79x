#include "system.h"
#include <stdio.h>
#include <string.h>
#include "hal/hal_display.h"

#define FONT_HEIGHT 13
#define FONT_WIDTH 7

#define CHARS_PER_ROW 20
#define NUMBER_OF_ROWS 5
static char textStr[NUMBER_OF_ROWS][CHARS_PER_ROW] = {""};
static systimer_t timer;

void init_display(void) {
    u8g_FirstPage(&u8g);
    do {
        u8g_SetDefaultBackgroundColor(&u8g);
        u8g_DrawBox(&u8g, 0, 0, 128, 64);
        u8g_SetDefaultForegroundColor(&u8g);
        u8g_SetFont(&u8g, u8g_font_7x13B);
    } while ( u8g_NextPage(&u8g) );
    systimer_start(&timer, 250);
}

void print_text(uint8_t row, char *str) {
    if(row < NUMBER_OF_ROWS) {
        strncpy(textStr[row], str, CHARS_PER_ROW);
    }
}

void clear_display(void) {
    int a;
    for(a=0;a<NUMBER_OF_ROWS;a++) {
        print_text(a, "");
    }
}

void task_display(void) {
    int a;
    if(systimer_is_expired(&timer)) {
        u8g_FirstPage(&u8g);
        do {
            for(a=0;a<NUMBER_OF_ROWS;a++) {
                u8g_DrawStr(&u8g,  0, FONT_HEIGHT*(a+1), textStr[a]);
            }
        } while ( u8g_NextPage(&u8g) );
        systimer_start(&timer, 250);
    }
}