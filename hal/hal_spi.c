#include "hal_spi.h"
#include "hal_mcu.h"
#include "system.h"

/* SPI pins bit-banged as plain GPIO. Shared bus (SCK/MISO/MOSI) between
   the display and the remote-com SPI-UART bridge, which each drive their
   own chip-select pin.
   MISO swapped from p0.17 to p2.7 (was remotecom's CS_PIN) to test whether
   CS/MISO were crossed in the wiring. */
#define sck  15
#define miso 7
#define mosi 18

void init_hal_spi(void)
{
  LPC_SC->PCONP &= ~((1 << 21) | (1 << 10)); //Disable SSP0 and SSP1 to avoid collision on shared pins

  LPC_PINCON->PINSEL0 &= ~((3 << 30));  // p0.15 -> gpio (sck)
  LPC_PINCON->PINSEL1 &= ~(3 << 4);     // p0.18 -> gpio (mosi)
  LPC_PINCON->PINSEL4 &= ~(3 << 14);    // p2.7 -> gpio (miso)
  LPC_PINCON->PINMODE4 |= (3 << 14);    // miso seems to work best with pulldown resistor

  LPC_GPIO0->FIODIR |= (1 << sck);   // p0.15 output mode.
  LPC_GPIO0->FIODIR |= (1 << mosi);  // p0.18 output mode.
  LPC_GPIO2->FIODIR &= ~(1 << miso); // p2.7 input mode.
  LPC_GPIO0->FIOCLR = (1 << sck);    // clock idle low (SPI mode 0)
}

/* Bit-banged SPI mode 0 (CPOL=0, CPHA=0), MSB first. */
uint8_t spi_transfer(uint8_t data)
{
  uint8_t received = 0;

  for (int8_t bit = 7; bit >= 0; bit--)
  {
    if (data & (1 << bit)) {
      LPC_GPIO0->FIOSET = (1 << mosi);
    } else {
      LPC_GPIO0->FIOCLR = (1 << mosi);
    }

    delay_micro_seconds(1);

    LPC_GPIO0->FIOSET = (1 << sck);
    if (LPC_GPIO2->FIOPIN & (1 << miso)) {
      received |= (1 << bit);
    }

    delay_micro_seconds(1);

    LPC_GPIO0->FIOCLR = (1 << sck);
  }

  return received;
}
