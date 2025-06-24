#include "hal_mcu.h"
#include "hal_charger.h"

#define CHARGER_CONNECTED_PORTNO    1
#define CHARGER_CONNECTED_PINNO    21
#define CHARGER_INITIATE_PORTNO     1
#define CHARGER_INITIATE_PINNO     23
#define CHARGER_CHARGE_PORTNO       0
#define CHARGER_CHARGE_PINNO       11

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
