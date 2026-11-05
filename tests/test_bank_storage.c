#include "bank_storage.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
uint8_t test_flash[2097152];
static uint8_t record[1024], backup[24576];
static RobotBank bank, loaded;
static bool paused, allowed = true, interrupted;
static unsigned writes;
static size_t limit = ROBOT_BANK_RECORD_SIZE;
bool audio_flash_pause(void) {
  if (!allowed)
    return false;
  assert(!paused);
  paused = true;
  return true;
}
void audio_flash_resume(void) {
  assert(paused);
  paused = false;
}
void flash_range_erase(uint32_t offset, size_t size) {
  assert(paused && offset >= ROBOT_CONFIG_FLASH_OFFSET &&
         offset + size <= sizeof(test_flash));
  assert(!(offset % 4096) && !(size % 4096));
  memset(test_flash + offset, 0xff, size);
}
void flash_range_program(uint32_t offset, const uint8_t *data, size_t size) {
  assert(paused && offset >= ROBOT_CONFIG_FLASH_OFFSET &&
         offset + size <= sizeof(test_flash));
  if (size > limit)
    size = limit;
  for (size_t i = 0; i < size; i++)
    test_flash[offset + i] &= data[i];
  writes++;
}
int flash_safe_execute(void (*fn)(void *), void *ctx, uint32_t timeout) {
  assert(timeout);
  fn(ctx);
  return interrupted ? -1 : 0;
}
int main(void) {
  memset(test_flash, 0xff, sizeof(test_flash));
  bool migrated = false;
  assert(!bank_storage_load(&loaded, &migrated));
  RobotConfig old = {0};
  robot_init(&old.robot);
  old.volume = 200;
  old.autoload = true;
  robot_set(&old.robot, "PITCH", 93);
  robot_pronounce(&old.robot, "operational", "opperational");
  assert(robot_config_encode(&old, 4, record));
  memcpy(test_flash + 2088960, record, sizeof(record));
  assert(bank_storage_load(&bank, &migrated) && migrated &&
         bank.volume == 200 && bank.characters[1].voice.pitch == 93);
  memcpy(backup, test_flash + ROBOT_CONFIG_FLASH_OFFSET, sizeof(backup));
  const unsigned cuts[] = {0,    1,    255,  256,  1023, 1024, 4095, 4096,
                           5415, 5416, 6140, 6141, 6142, 6143, 6144};
  bank.volume = 300;
  interrupted = true;
  for (unsigned i = 0; i < sizeof(cuts) / sizeof(cuts[0]); i++) {
    memcpy(test_flash + ROBOT_CONFIG_FLASH_OFFSET, backup, sizeof(backup));
    limit = cuts[i];
    assert(bank_storage_save(&bank) == -1);
    assert(!paused);
    assert(bank_storage_load(&loaded, &migrated));
    assert(migrated == (limit < ROBOT_BANK_RECORD_SIZE));
    assert(loaded.volume == (limit < ROBOT_BANK_RECORD_SIZE ? 200 : 300));
    assert(!memcmp(test_flash + 2088960, record, sizeof(record)));
  }
  interrupted = false;
  limit = ROBOT_BANK_RECORD_SIZE;
  assert(bank_storage_save(&bank) == 0);
  unsigned count = writes;
  assert(bank_storage_save(&bank) == 0 && writes == count);
  bank.volume = 400;
  assert(bank_storage_save(&bank) == 1);
  assert(bank_storage_load(&loaded, &migrated) && !migrated &&
         loaded.volume == 400);
  allowed = false;
  bank.volume = 100;
  assert(bank_storage_save(&bank) == -1 && !bank_storage_erase());
  allowed = true;
  assert(bank_storage_load(&loaded, &migrated) && loaded.volume == 400);
  assert(bank_storage_erase());
  assert(!bank_storage_load(&loaded, &migrated));
  for (unsigned i = 0; i < 24576; i++)
    assert(test_flash[ROBOT_CONFIG_FLASH_OFFSET + i] == 0xff);
  puts("Bank storage checks passed: non-destructive 0.4.0 migration, "
       "partial-program fallback, alternating slots, no-op writes, audio "
       "guard, erase without legacy resurrection.");
}
