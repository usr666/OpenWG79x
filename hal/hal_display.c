#include "LPC17xx.h"
#include "u8g.h"
#include "system.h"
#include <stdbool.h>

u8g_t u8g;

//todo: fix better defines with comments.
#define rstb 19
#define csb  16
#define a0   20

uint8_t u8g_com_hw_spi_fn(u8g_t *u8g, uint8_t msg, uint8_t arg_val, void *arg_ptr);

void init_hal_display(void) {

// Configure SPI (LCD)
  LPC_SC->PCLKSEL0 |= (1 << 16);  // set SPI CCLK

  LPC_GPIO1->FIODIR |= (1 << 20);   // P1.20 output mode.
  LPC_GPIO0->FIODIR |= (1 << rstb); // p0.19 output mode.
  LPC_GPIO0->FIODIR |= (1 << csb);  // p0.16 output mode.
  LPC_GPIO0->FIODIR |= (1 << a0);   // p0.20 output mode.

  LPC_PINCON->PINSEL0 |= (1 << 30) | (1 << 31); // p0.15 -> sck
  LPC_PINCON->PINMODE0 |= (1 << 30);            // p0.15 Repeater mode *todo: need more checking

  LPC_PINCON->PINSEL1 |= ( 0xc | 0x30 );  // p0.17 & p0.18 miso / mosi (no miso??)
  LPC_PINCON->PINMODE1 |= ( (1 << 0) | (1 << 2) | (1 << 4));  // p0.16 p0.17 p0.18  repeater mode *todo: need more checking
  LPC_PINCON->PINMODE1 |= (1 << 6);   // p0.19 repeater mode *todo: need more checking
 
  LPC_SPI->SPCR |= (1 << 5);  // SPI operates in Master mode.


//Configur u8g
  //u8g_InitComFn(&u8g, &u8g_dev_st7565_nhd_c12864_hw_spi, u8g_com_hw_spi_fn);
  u8g_InitComFn(&u8g, &u8g_dev_st7565_nhd_c12864_2x_hw_spi, u8g_com_hw_spi_fn); 
  u8g_SetContrast(&u8g, 4 );
  u8g_SetDefaultForegroundColor(&u8g);
  u8g_SetRot180(&u8g);

}

void set_backlight(bool on)
{
  if(on) {
    LPC_GPIO1->FIOSET = ( 1 << 20 );  // p1.20 LCD backlight ON
  } else {
    LPC_GPIO1->FIOCLR = ( 1 << 20 );  // p1.20 LCD backlight ON
  }
}
void spi_out(uint8_t data)
{
  LPC_SPI->SPDR = data;
  while ( ( (LPC_SPI->SPSR >> 7 ) & 1 ) == 0 )
    ;
}

void u8g_Delay(uint16_t val)
{
  delay_micro_seconds(1000UL*(uint32_t)val);
}

void u8g_MicroDelay(void)
{
  delay_micro_seconds(1);
}

uint8_t u8g_com_hw_spi_fn(u8g_t *u8g, uint8_t msg, uint8_t arg_val, void *arg_ptr)
{
  switch(msg)
  {
    case U8G_COM_MSG_STOP:
      break;
    
    case U8G_COM_MSG_INIT:
    // init spi and ports
      u8g_MicroDelay();      
      break;
    
    case U8G_COM_MSG_ADDRESS:                     /* define cmd (arg_val = 0) or data mode (arg_val = 1) */
      //u8g_10MicroDelay();
      if (arg_val) { LPC_GPIO0->FIOPIN |= ( 1 << a0); } else { LPC_GPIO0->FIOPIN &= ~( 1 << a0); }
      u8g_MicroDelay();
     break;

    case U8G_COM_MSG_CHIP_SELECT:
      if (!arg_val) { LPC_GPIO0->FIOPIN |= ( 1 << csb); } else { LPC_GPIO0->FIOPIN &= ~( 1 << csb); }
      u8g_MicroDelay();
      break;
      
    case U8G_COM_MSG_RESET:
      if (arg_val) { LPC_GPIO0->FIOPIN |= ( 1 << rstb); } else { LPC_GPIO0->FIOPIN &= ~( 1 << rstb); }
      u8g_MicroDelay();
      break;
      
    case U8G_COM_MSG_WRITE_BYTE:
      spi_out(arg_val);
      u8g_MicroDelay();
      break;
    
    case U8G_COM_MSG_WRITE_SEQ:
    case U8G_COM_MSG_WRITE_SEQ_P:
      {
        register uint8_t *ptr = arg_ptr;
        while( arg_val > 0 )
        {
          spi_out(*ptr++);
          arg_val--;
        }
      }
      break;
  }
  return 1;
}