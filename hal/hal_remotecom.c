#include "hal_remotecom.h"
#include "hal_mcu.h"
#include "hal_spi.h"
#include "system.h"

#define CS_PIN 17 // swapped from p2.7 to p0.17 (was hal_spi's miso) to test whether CS/MISO were crossed in the wiring

#define SC16IS750_RHR 0x00
#define SC16IS750_THR 0x00
#define SC16IS750_DLL 0x00
#define SC16IS750_IER 0x01
#define SC16IS750_DLH 0x01
#define SC16IS750_FCR 0x02
#define SC16IS750_LCR 0x03
#define SC16IS750_MCR 0x04
#define SC16IS750_LSR 0x05
#define SC16IS750_MSR 0x06
#define SC16IS750_SPR 0x07
#define SC16IS750_TXLVL 0x08
#define SC16IS750_RXLVL 0x09

#define SC16IS750_READ 0x80
#define SC16IS750_WRITE 0x00
#define SC16IS750_CHANNEL 0

#define BAUDRATE 38400

static void cs_select(void)
{
    LPC_GPIO0->FIOCLR = (1 << CS_PIN);
    delay_micro_seconds(20); // workaround for slow rise/fall time on p0.17
}

static void cs_deselect(void)
{
    LPC_GPIO0->FIOSET = (1 << CS_PIN);
    delay_micro_seconds(20); // workaround for slow rise/fall time on p0.17
}

static uint8_t sc16is750_read(uint8_t reg)
{
    uint8_t addr = (reg << 3) | (SC16IS750_CHANNEL << 1) | SC16IS750_READ;
    uint8_t data;
    
    cs_select();
    spi_transfer(addr);
    data = spi_transfer(0x00);
    cs_deselect();
    
    return data;
}

static void sc16is750_write(uint8_t reg, uint8_t data)
{
    uint8_t addr = (reg << 3) | (SC16IS750_CHANNEL << 1) | SC16IS750_WRITE;
    
    cs_select();
    spi_transfer(addr);
    spi_transfer(data);
    cs_deselect();
}

void init_hal_remotecom(void)
{
    /* Configure CS pin as output */
    LPC_PINCON->PINSEL1 &= ~(3 << 2); // p0.17 -> gpio (cs)
    LPC_GPIO0->FIODIR |= (1 << CS_PIN);
    cs_deselect();
    
    delay_micro_seconds(100);
    
    /* Clear TX and RX FIFOs */
    sc16is750_write(SC16IS750_FCR, 0x07);
    
    /* Enable divisor access (DLAB = 1) */
    sc16is750_write(SC16IS750_LCR, 0x80);
    
    /* Set baud rate divisor */
    /* Crystal on SPI bridge is 14.7456 MHz; divisor = crystal / (baud * 16) */
    uint16_t divisor = (14745600UL / (BAUDRATE * 16));
    sc16is750_write(SC16IS750_DLL, divisor & 0xFF);
    sc16is750_write(SC16IS750_DLH, (divisor >> 8) & 0xFF);
    
    /* 8N1: 8 bits, no parity, 1 stop bit (DLAB = 0) */
    sc16is750_write(SC16IS750_LCR, 0x03);
    
    /* Clear FIFOs */
    sc16is750_write(SC16IS750_FCR, 0x07);
}

bool remotecom_send_byte(uint8_t data)
{
    uint8_t lsr = sc16is750_read(SC16IS750_LSR);
    
    if ((lsr & 0x20) == 0x20) {
        sc16is750_write(SC16IS750_THR, data);
        return true;
    }
    return false;
}

bool remotecom_recv_byte(uint8_t *data)
{
    uint8_t lsr = sc16is750_read(SC16IS750_LSR);
    
    if ((lsr & 0x01) == 0x01) {
        *data = sc16is750_read(SC16IS750_RHR);
        return true;
    }
    return false;
}


