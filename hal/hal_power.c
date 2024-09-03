#include "hal_mcu.h"
#include "hal_power.h"
#include <stdbool.h>

#define POWERON_PORTNO   1
#define POWERON_PINNO    25 // Set this pin to keep power to cpu

void init_hal_power(void) {
  LPC_GPIOx(POWERON_PORTNO)->FIODIR |= ( 1 << POWERON_PINNO );
  LPC_GPIOx(POWERON_PORTNO)->FIOSET = ( 1 << POWERON_PINNO );
}

void poweroff(void)
{
  LPC_GPIOx(POWERON_PORTNO)->FIOCLR = ( 1 << POWERON_PINNO );
}
