#include "u8g.h"
#include "system.h"

void init_display(void) {

}

void task_display(void) {
    u8g_FirstPage(&u8g);
    do {
      u8g_SetFont(&u8g, u8g_font_tpss);
      u8g_DrawStr(&u8g,  0, 25, "Hello World!");
    } while ( u8g_NextPage(&u8g) );
}
