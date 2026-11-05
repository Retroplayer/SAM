#include "bank_storage.h"
#include "audio.h"
#include "hardware/flash.h"
#include "pico/flash.h"
#include "pico/stdlib.h"
#include <string.h>

#define SLOT_SIZE 8192u
#define LEGACY_OFFSET (2097152u - 8192u)
_Static_assert(PICO_FLASH_SIZE_BYTES == 2097152,
               "Bank layout requires 2 MiB flash");
_Static_assert(ROBOT_BANK_RECORD_SIZE % FLASH_PAGE_SIZE == 0,
               "Record alignment");
static uint8_t record[ROBOT_BANK_RECORD_SIZE] __attribute__((aligned(4)));
static RobotConfig legacy;
static const uint8_t *slot(unsigned id) {
  return (const uint8_t *)(XIP_BASE + ROBOT_CONFIG_FLASH_OFFSET +
                           id * SLOT_SIZE);
}
bool bank_storage_load(RobotBank *bank, bool *migrated) {
  if (robot_bank_select(slot(0), slot(1), bank, NULL) >= 0) {
    if (migrated)
      *migrated = false;
    return true;
  }
  const uint8_t *a = (const uint8_t *)(XIP_BASE + LEGACY_OFFSET);
  if (robot_config_select(a, a + FLASH_SECTOR_SIZE, &legacy, NULL) < 0)
    return false;
  robot_bank_migrate(&legacy, bank);
  if (migrated)
    *migrated = true;
  return true;
}
typedef struct {
  uint32_t offset, length;
  bool erase_only;
} Write;
static void __not_in_flash_func(write_bank)(void *context) {
  Write *w = context;
  flash_range_erase(w->offset, w->length);
  if (!w->erase_only)
    flash_range_program(w->offset, record, sizeof(record));
}
int bank_storage_save(const RobotBank *bank) {
  uint32_t seq = 0;
  int active = robot_bank_select(slot(0), slot(1), NULL, &seq);
  if (!robot_bank_encode(bank, active < 0 ? 1 : seq + 1, record))
    return -1;
  if (active >= 0 &&
      !memcmp(slot((unsigned)active) + 16, record + 16, sizeof(record) - 16))
    return 0;
  Write w = {ROBOT_CONFIG_FLASH_OFFSET + (active == 0 ? SLOT_SIZE : 0),
             SLOT_SIZE, false};
  if (!audio_flash_pause())
    return -1;
  int status = flash_safe_execute(write_bank, &w, 1000);
  audio_flash_resume();
  if (status != PICO_OK ||
      memcmp(slot(active == 0 ? 1 : 0), record, sizeof(record)))
    return -1;
  return 1;
}
bool bank_storage_erase(void) {
  Write w = {ROBOT_CONFIG_FLASH_OFFSET, 24576, true};
  if (!audio_flash_pause())
    return false;
  int status = flash_safe_execute(write_bank, &w, 1000);
  audio_flash_resume();
  const uint8_t *p = slot(0);
  for (unsigned i = 0; i < w.length; i++)
    if (p[i] != 0xff)
      return false;
  return status == PICO_OK;
}
