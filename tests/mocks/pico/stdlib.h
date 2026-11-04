#ifndef MOCK_PICO_STDLIB_H
#define MOCK_PICO_STDLIB_H
#include <stdint.h>
#define PICO_FLASH_SIZE_BYTES 2097152
extern uint8_t test_flash[PICO_FLASH_SIZE_BYTES];
#define XIP_BASE ((uintptr_t)test_flash)
#define __not_in_flash_func(name) name
#define PICO_OK 0
#endif
