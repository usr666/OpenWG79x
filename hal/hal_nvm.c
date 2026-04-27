#include "hal_nvm.h"

#include <LPC17xx.h>

static uint32_t hal_nvm_checksum(const uint8_t *buffer)
{
    uint32_t hash = 2166136261UL;
    uint32_t i;

    for (i = 0; i < HAL_NVM_BUFFER_SIZE; i++) {
        hash ^= buffer[i];
        hash *= 16777619UL;
    }

    return hash;
}

void hal_nvm_store(const uint8_t *buffer)
{
    LPC_RTC->GPREG0 = *(uint32_t *)&buffer[0];
    LPC_RTC->GPREG1 = *(uint32_t *)&buffer[4U];
    LPC_RTC->GPREG2 = *(uint32_t *)&buffer[8U];
    LPC_RTC->GPREG3 = *(uint32_t *)&buffer[12U];
    LPC_RTC->GPREG4 = hal_nvm_checksum(buffer);
}

bool hal_nvm_load(uint8_t *buffer)
{
    *(uint32_t *)&buffer[0U] = LPC_RTC->GPREG0;
    *(uint32_t *)&buffer[4U] = LPC_RTC->GPREG1;
    *(uint32_t *)&buffer[8U] = LPC_RTC->GPREG2;
    *(uint32_t *)&buffer[12U] = LPC_RTC->GPREG3;

    if (LPC_RTC->GPREG4 != hal_nvm_checksum(buffer)) {
        return false;
    }

    return true;
}
