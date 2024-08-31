#include "LPC17xx.h"
#include "hal_keyboard.h"
#include "system.h"
#include <stdbool.h>

#define KEY_R1_PORTNO  1
#define KEY_R1_PINNO   0
#define KEY_R2_PORTNO  1
#define KEY_R2_PINNO   1
#define KEY_R3_PORTNO  1
#define KEY_R3_PINNO   4
#define KEY_R4_PORTNO  1
#define KEY_R4_PINNO   8
#define KEY_C1_PORTNO  1
#define KEY_C1_PINNO   9
#define KEY_C2_PORTNO  1
#define KEY_C2_PINNO   10
#define KEY_C3_PORTNO  1
#define KEY_C3_PINNO   14
#define KEY_C4_PORTNO  1
#define KEY_C4_PINNO   15

#define KEY_R_PINNO(x) (x==1 ? KEY_R1_PINNO : (x==2 ? KEY_R2_PINNO : (x==3 ? KEY_R3_PINNO : KEY_R4_PINNO)))

#define KEY_STOP_PORTNO   1
#define KEY_STOP_PINNO    17 // 1 if button pressed
#define KEY_POWER_PORTNO  1
#define KEY_POWER_PINNO   28 // 0 if button pressed

static keys_t pressedkey;

void init_hal_keyboard(void) {
    // Power- and Stop-button seems to be good with default port settings.

    // Set key rows as output, values high
    LPC_GPIO1->FIODIR |= ((1 << KEY_R1_PINNO) | (1 << KEY_R2_PINNO) | (1 << KEY_R3_PINNO) | (1 << KEY_R4_PINNO)); // ASSUMES all signals are on GPIO1
    LPC_GPIO1->FIOSET = ((1 << KEY_R1_PINNO) | (1 << KEY_R2_PINNO) | (1 << KEY_R3_PINNO) | (1 << KEY_R4_PINNO));  // ASSUMES all signals are on GPIO1

}

uint32_t getKeyboardDebugvalue(void)
{
    return LPC_GPIO1->FIOPIN;
}

bool get_stop_button(void)
{
    if(((LPC_GPIO1->FIOPIN) & (1 << KEY_STOP_PINNO)) > 0) {
        return true;
    } else {
        return false;
    }
}

bool get_power_button(void)
{
    if(((LPC_GPIO1->FIOPIN) & (1 << KEY_POWER_PINNO)) == 0) {
        return true;
    } else {
        return false;
    }
}

keys_t get_pressed_key(void)
{
    return pressedkey;
}

static keys_t keylookup[4][4] = {   
    { KEY8,     KEY9,  KEY0,    KEYSTART },
    { KEY6,     KEYOK, KEYDOWN, KEY7 },
    { KEYBACK,  KEYUP, KEY4,    KEY5 },
    { KEYHOME,  KEY1,  KEY2,    KEY3 }
};

// This function needs to be called 4 times to read all keys.
// Should be called with a minimum interval of 1ms and max interval of 50ms
static void internalkeyboardtask(void)
{
    static uint8_t activerow=0;
    static uint8_t localpressedkey = KEY_NONE;

    uint32_t input = LPC_GPIO1->FIOPIN;
    if((input & (1 << KEY_C1_PINNO)) == 0) {
        localpressedkey = keylookup[activerow][0];
    } else if((input & (1 << KEY_C2_PINNO)) == 0) {
        localpressedkey = keylookup[activerow][1];
    } else if((input & (1 << KEY_C3_PINNO)) == 0) {
        localpressedkey = keylookup[activerow][2];
    } else if((input & (1 << KEY_C4_PINNO)) == 0) {
        localpressedkey = keylookup[activerow][3];
    }
    activerow++;
    if(activerow >= 4) {
        activerow = 0;
        pressedkey = localpressedkey;
        localpressedkey = KEY_NONE;
    }

    // Reset all outputs high
    LPC_GPIO1->FIOSET = ((1 << KEY_R1_PINNO) | (1 << KEY_R2_PINNO) | (1 << KEY_R3_PINNO) | (1 << KEY_R4_PINNO));  // ASSUMES all signals are on GPIO1
    // Set current row low
    LPC_GPIO1->FIOCLR = (1 << KEY_R_PINNO(activerow+1));  // ASSUMES all signals are on GPIO1

}

void task_keyboard(void)
{
    int i;
    for(i=0 ; i<4 ; i++) {
        internalkeyboardtask();
        delay_micro_seconds(1000);
    }
}
