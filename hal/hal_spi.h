#ifndef _HAL_SPI_H
#define _HAL_SPI_H

#include <stdint.h>

void init_hal_spi(void);
uint8_t spi_transfer(uint8_t data);

#endif
