#ifndef _HAL_NVM_H
#define _HAL_NVM_H

#include <stdbool.h>
#include <stdint.h>

#define HAL_NVM_BUFFER_SIZE 16U

void hal_nvm_store(const uint8_t *buffer);
bool hal_nvm_load(uint8_t *buffer);

#endif
