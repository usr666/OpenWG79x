#include "hal_mcu.h"
#include "hal_charger.h"
#include "hal_adc.h"

#define CHARGER_CONNECTED_PORTNO    1
#define CHARGER_CONNECTED_PINNO    21
#define CHARGER_INITIATE_PORTNO     1
#define CHARGER_INITIATE_PINNO     23
#define CHARGER_CHARGE_PORTNO       0
#define CHARGER_CHARGE_PINNO       11
#define BATTERY_VOLTAGE_PINNO       1

void init_hal_charger(void) {
  LPC_GPIOx(CHARGER_INITIATE_PORTNO)->FIODIR |= ( 1 << CHARGER_INITIATE_PINNO );
  LPC_GPIOx(CHARGER_CHARGE_PORTNO)->FIODIR |= ( 1 << CHARGER_CHARGE_PINNO );
  set_charger_initiate(true); // Set this so that mower charges battery if inserted into charging station
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
  // First primitive detection of fully charged battery. TODO: improve
  return(get_battery_soc() > 95);
}

// return battery voltage in mV
uint32_t get_battery_voltage(void) {
  return(hal_adc_get_value(BATTERY_VOLTAGE_PINNO) * 1); // todo: adjust scale
}

#define BATTERY_VOLTAGE_EMPTY 21000
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
