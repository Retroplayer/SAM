#ifndef ROBOT_PGMSPACE_H
#define ROBOT_PGMSPACE_H
#include <stdint.h>
/* RP2040 and desktop targets have memory-mapped read-only data. */
#define PROGMEM
#define pgm_read_byte(address) (*(const uint8_t *)(address))
#endif
