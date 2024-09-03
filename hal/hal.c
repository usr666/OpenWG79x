#include "hal_mcu.h"
#include "system.h"

void init_hal(void) {
  
// Setup Systick
  SysTick->LOAD = (SystemCoreClock/1000UL*(unsigned long)SYS_TICK_PERIOD_IN_MS) - 1;
  SysTick->VAL = 0;
  SysTick->CTRL = 7; // enable, generate interrupt (SysTick_Handler), do not divide by 2

// Power cntrl
  LPC_SC->PCONP |= (1 << 15);   // power up GPIO
  LPC_SC->PCONP |= (1 << 8);    // power up SPI

 
}
