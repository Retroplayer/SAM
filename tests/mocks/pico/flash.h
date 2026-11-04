#ifndef MOCK_SAFE_FLASH_H
#define MOCK_SAFE_FLASH_H
#include <stdint.h>
int flash_safe_execute(void (*callback)(void *),void *context,uint32_t timeout_ms);
#endif
