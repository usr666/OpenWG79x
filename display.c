#include "u8g.h"
#include "system.h"
#include <stdio.h>
#include <string.h>
#include "LPC17xx.h" // temp debug, use hal instead


void init_display(void) {

}

void task_display(void) {
    char buffer[64];
    u8g_FirstPage(&u8g);
    do {
        u8g_SetDefaultBackgroundColor(&u8g);
        u8g_DrawBox(&u8g, 0, 0, 128, 64);
        u8g_SetDefaultForegroundColor(&u8g);
        u8g_SetFont(&u8g, u8g_font_4x6);
        //sprintf(buffer, "Kdn:%u d1:%u d2:%u d3:%u d4:%u", keypadGetKey(), debug1, debug2, debug3, debug4);
        // binary debug
        //u8g_DrawStr(&u8g,  0, 10, buffer);
        u8g_DrawStr(&u8g,  0, 20, "01234567890123456789012345678901");
        for (uint8_t i = 0 ; i < 32 ; i++) {
            if ((LPC_GPIO0->FIOPIN >> i) & 1) {
                buffer[i] = 0x31;
            } else {
                buffer[i] = 0x30;
            }
        }
        buffer[32] = 0x0;
        u8g_DrawStr(&u8g,  0, 30, buffer);
        for (uint8_t i = 0 ; i < 32 ; i++) {
            if ((LPC_GPIO1->FIOPIN >> i) & 1) {
                buffer[i] = 0x31;
            } else {
                buffer[i] = 0x30;
            }
        }
        buffer[32] = 0x0;
        u8g_DrawStr(&u8g,  0, 40, buffer);
        for (uint8_t i = 0 ; i < 32 ; i++) {
            if ((LPC_GPIO2->FIOPIN >> i) & 1) {
                buffer[i] = 0x31;
            } else {
                buffer[i] = 0x30;
            }
            if (i >= 14) buffer[i] = 0x5f;

            if (i == 20) buffer[i] = ((LPC_GPIO3->FIOPIN >> 25) & 1) ? 0x31 : 0x30;
            if (i == 21) buffer[i] = ((LPC_GPIO3->FIOPIN >> 26) & 1) ? 0x31 : 0x30;
            if (i == 23) buffer[i] = ((LPC_GPIO4->FIOPIN >> 28) & 1) ? 0x31 : 0x30;
            if (i == 24) buffer[i] = ((LPC_GPIO4->FIOPIN >> 29) & 1) ? 0x31 : 0x30;
        }
        buffer[32] = 0x0;
        u8g_DrawStr(&u8g,  0, 48, buffer);
        /*
        sprintf(buffer, "A");
        sprintf(buffer + strlen(buffer), "0:%04lu ", ADC0);
        sprintf(buffer + strlen(buffer), "1:%04lu ", ADC1);
        sprintf(buffer + strlen(buffer), "2:%04lu ", ADC2);
        sprintf(buffer + strlen(buffer), "3:%04lu", ADC3);
        u8g_DrawStr(&u8g,  0, 56, buffer);
        sprintf(buffer, "B");
        sprintf(buffer + strlen(buffer), "4:%04lu ", ADC4);
        sprintf(buffer + strlen(buffer), "5:%04lu ", ADC5);
        sprintf(buffer + strlen(buffer), "6:%04lu ", ADC6);
        sprintf(buffer + strlen(buffer), "7:%04lu", ADC7);
        buffer[32] = 0x0;

        u8g_DrawStr(&u8g,  0, 64, buffer);
        */
    } while ( u8g_NextPage(&u8g) );
}
