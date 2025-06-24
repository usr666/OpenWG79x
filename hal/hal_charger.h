#ifndef _HAL_CHARGER_H
#define _HAL_CHARGER_H
#include <stdbool.h>

void init_hal_charger(void);
bool get_charger_connected(void);
void set_charger_initiate(bool state);
void set_charger_active(bool state);
 
#endif