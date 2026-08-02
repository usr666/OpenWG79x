#ifndef _HAL_REMOTECOM_H
#define _HAL_REMOTECOM_H

#include <stdint.h>
#include <stdbool.h>

void init_hal_remotecom(void);
bool remotecom_send_byte(uint8_t data);
bool remotecom_recv_byte(uint8_t *data);

#endif
