#include "hal_mcu.h"
#include "hal_charger.h"
#include "hal_adc.h"
#include "system.h"

#define CHARGER_CONNECTED_PORTNO    1
#define CHARGER_CONNECTED_PINNO    21
#define CHARGER_INITIATE_PORTNO     1
#define CHARGER_INITIATE_PINNO     23
#define CHARGER_CHARGE_PORTNO       0
#define CHARGER_CHARGE_PINNO       11
#define BATTERY_VOLTAGE_PINNO       1
#define CHARGE_CURRENT_PINNO        0
#define MIN_CHARGE_CURRENT_MA     250
#define TIME_BELOW_MIN_CURRENT_MS 20000UL
#define MAX_TOTAL_CHARGE_TIME_MS 10800000UL

static systimer_t total_charge_time;
static systimer_t time_below_min_charge_current;

void init_hal_charger(void) {
  LPC_GPIOx(CHARGER_INITIATE_PORTNO)->FIODIR |= ( 1 << CHARGER_INITIATE_PINNO );
  LPC_GPIOx(CHARGER_CHARGE_PORTNO)->FIODIR |= ( 1 << CHARGER_CHARGE_PINNO );
  systimer_start(&total_charge_time, MAX_TOTAL_CHARGE_TIME_MS);
  systimer_start(&time_below_min_charge_current, TIME_BELOW_MIN_CURRENT_MS);
}

void task_hal_charger(void) {
  if(get_charger_connected()) {
    if(get_charge_current() > MIN_CHARGE_CURRENT_MA) {
      systimer_start(&time_below_min_charge_current, TIME_BELOW_MIN_CURRENT_MS);
    }
  } else {
    systimer_start(&total_charge_time, MAX_TOTAL_CHARGE_TIME_MS);
    systimer_start(&time_below_min_charge_current, TIME_BELOW_MIN_CURRENT_MS);
  }
}

bool get_charger_connected(void) {
  return ((((LPC_GPIOx(CHARGER_CONNECTED_PORTNO)->FIOPIN) & (1 << CHARGER_CONNECTED_PINNO))) == 0 ? false : true);
}

void set_charger_initiate(bool state) {
  if(state) {
    LPC_GPIOx(CHARGER_INITIATE_PORTNO)->FIOSET = ( 1 << CHARGER_INITIATE_PINNO );
  } else {
    LPC_GPIOx(CHARGER_INITIATE_PORTNO)->FIOCLR = ( 1 << CHARGER_INITIATE_PINNO );
  }
}

void set_charger_active(bool state) {
  if(state) {
    LPC_GPIOx(CHARGER_CHARGE_PORTNO)->FIOSET = ( 1 << CHARGER_CHARGE_PINNO );
  } else {
    LPC_GPIOx(CHARGER_CHARGE_PORTNO)->FIOCLR = ( 1 << CHARGER_CHARGE_PINNO );
  }
}

bool get_charge_complete(void){
  if(systimer_is_expired(&total_charge_time) || systimer_is_expired(&time_below_min_charge_current)) {
    return true;
  }
  return false;
}

#define BATTERY_MICROVOLT_PER_BIT 7548

// return battery voltage in mV
uint32_t get_battery_voltage(void) {
  uint32_t res;
  res = hal_adc_get_value(BATTERY_VOLTAGE_PINNO);
  res = res * (uint32_t)BATTERY_MICROVOLT_PER_BIT;
  res = res / (uint32_t)1000;
  return(res);
}

#define BATTERY_VOLTAGE_EMPTY 19600
#define BATTERY_VOLTAGE_FULL  29400
// return battery state of charge in percent
uint8_t get_battery_soc(void) {
  uint32_t batteryvoltage;
  batteryvoltage = get_battery_voltage();
  if(batteryvoltage < BATTERY_VOLTAGE_EMPTY) {
    return 0;
  } else if(batteryvoltage > BATTERY_VOLTAGE_FULL) {
    return 100;
  }
  return((batteryvoltage - BATTERY_VOLTAGE_EMPTY) * 100 / (BATTERY_VOLTAGE_FULL - BATTERY_VOLTAGE_EMPTY));
}

#define CHARGE_CURRENT_OFFSET 256
#define CHARGE_CURRENT_MICROAMPS_PER_BIT 896 // 2240
// return charge current in mA
uint32_t get_charge_current(void) {
  uint32_t res;
  res = hal_adc_get_value(CHARGE_CURRENT_PINNO);
  if(res > CHARGE_CURRENT_OFFSET) {
    res -= CHARGE_CURRENT_OFFSET;
  } else {
    res = 0;
  }
  res = res * CHARGE_CURRENT_MICROAMPS_PER_BIT;
  return(res/(uint32_t)1000);
}