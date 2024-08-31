#ifndef _HAL_DISPLAY_H
#define _HAL_DISPLAY_H
#include "u8g.h"
#include <stdbool.h>

void init_hal_display(void);
void set_backlight(bool on);

extern u8g_t u8g;
  
#endif