#ifndef _HAL_CHARGER_H
#define _HAL_CHARGER_H
#include <stdbool.h>
#include <stdint.h>

void init_hal_charger(void);
void task_hal_charger(void);
bool get_charger_connected(void);
void set_charger_initiate(bool state);
void set_charger_active(bool state);
uint32_t get_battery_voltage(void);
uint8_t get_battery_soc(void);
bool get_charge_complete(void);
uint32_t get_charge_current(void);

#endif