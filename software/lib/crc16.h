#ifndef CRC16_H
#define CRC16_H

#include <stddef.h>
#include <stdint.h>

/* CRC-16/MODBUS: poly 0xA001 (reflected 0x8005), init 0xFFFF. */
uint16_t crc16_modbus(const uint8_t *data, size_t len);

#endif
