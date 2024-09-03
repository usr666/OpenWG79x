#include "system.h"
#include "hal/hal_mcu.h"


uint32_t systick_cnt __attribute__((aligned(4))) = 0; // If variable is 4-byte aligned read/writes are atomic

static void _delay_system_ticks_sub(uint32_t sys_ticks)
{
  uint32_t start_val, end_val, curr_val;
  uint32_t load;
  
  start_val = SysTick->VAL;
  start_val &= 0x0ffffffUL;
  end_val = start_val;
  
  if ( end_val < sys_ticks )
  {
    /* check, if the operation after this if clause would lead to a negative result */
    /* if this would be the case, then add the reload value first */
    load = SysTick->LOAD;
    load &= 0x0ffffffUL;
    end_val += load;
  }
  /* counter goes towards zero, so end_val is below start value */
  end_val -= sys_ticks;		
  
  
  /* wait until interval is left */
  if ( start_val >= end_val )
  {
    for(;;)
    {
      curr_val = SysTick->VAL;
      curr_val &= 0x0ffffffUL;
      if ( curr_val <= end_val )
	break;
      if ( curr_val > start_val )
	break;
    }
  }
  else
  {
    for(;;)
    {
      curr_val = SysTick->VAL;
      curr_val &= 0x0ffffffUL;
      if ( curr_val <= end_val && curr_val > start_val )
	break;
    }
  }
}

/*
  Delay by the provided number of system ticks.
  Any values between 0 and 0x0ffffffff are allowed.
*/
void delay_system_ticks(uint32_t sys_ticks)
{
  uint32_t load4;
  load4 = SysTick->LOAD;
  load4 &= 0x0ffffffUL;
  load4 >>= 2;
  
  while ( sys_ticks > load4 )
  {
    sys_ticks -= load4;
    _delay_system_ticks_sub(load4);
  }
  _delay_system_ticks_sub(sys_ticks);
}

/*
  Delay by the provided number of micro seconds.
  Limitation: "us" * System-Freq in MHz must now overflow in 32 bit.
  Values between 0 and 1.000.000 (1 second) are ok.
*/
void delay_micro_seconds(uint32_t us)
{
  uint32_t sys_ticks;

  sys_ticks = SystemCoreClock;
  sys_ticks /=1000000UL;
  sys_ticks *= us;
  delay_system_ticks(sys_ticks);  
} 

void __attribute__ ((interrupt)) SysTick_Handler(void)
{
  systick_cnt++;
} 