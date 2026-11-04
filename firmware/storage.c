#include "storage.h"
#include "audio.h"
#include "hardware/flash.h"
#include "pico/flash.h"
#include "pico/stdlib.h"
#include <string.h>

_Static_assert(PICO_FLASH_SIZE_BYTES==2097152,"Config layout requires the Zero's 2 MiB flash");
_Static_assert(ROBOT_CONFIG_RECORD_SIZE%FLASH_PAGE_SIZE==0,"Record must be page aligned");
static uint8_t record[ROBOT_CONFIG_RECORD_SIZE] __attribute__((aligned(4)));
static bool present;
static const uint8_t *slot(unsigned index) {
    return (const uint8_t *)(XIP_BASE+ROBOT_CONFIG_FLASH_OFFSET+index*FLASH_SECTOR_SIZE);
}
bool storage_load(RobotConfig *config) {
    present=robot_config_select(slot(0),slot(1),config,NULL)>=0;
    return present;
}
typedef struct {uint32_t offset;bool erase_all;} Operation;
static void __not_in_flash_func(write_flash)(void *context) {
    Operation *op=context;
    flash_range_erase(op->offset,op->erase_all?2*FLASH_SECTOR_SIZE:FLASH_SECTOR_SIZE);
    if (!op->erase_all) flash_range_program(op->offset,record,sizeof(record));
}
int storage_save(const RobotConfig *config) {
    uint32_t sequence=0;
    int active=robot_config_select(slot(0),slot(1),NULL,&sequence);
    if (!robot_config_encode(config,active<0?1:sequence+1,record)) return -1;
    if (active>=0 && !memcmp(record+16,slot((unsigned)active)+16,sizeof(record)-16)) {
        present=true;return 0;
    }
    Operation op={ROBOT_CONFIG_FLASH_OFFSET+(active==0?FLASH_SECTOR_SIZE:0),false};
    if (!audio_flash_pause()) return -1;
    int result=flash_safe_execute(write_flash,&op,1000);
    audio_flash_resume();
    present=robot_config_select(slot(0),slot(1),NULL,NULL)>=0;
    if (result!=PICO_OK || memcmp(slot(active==0?1:0),record,sizeof(record))) return -1;
    return 1;
}
bool storage_erase(void) {
    Operation op={ROBOT_CONFIG_FLASH_OFFSET,true};
    if (!audio_flash_pause()) return false;
    int result=flash_safe_execute(write_flash,&op,1000);
    audio_flash_resume();
    present=robot_config_select(slot(0),slot(1),NULL,NULL)>=0;
    return result==PICO_OK && !present;
}
bool storage_present(void) {return present;}
