/* Execute the actual firmware storage adapter against simulated NOR flash. */
#include "storage.h"
#include "robot_pcm.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

uint8_t test_flash[2097152];
static uint8_t backup[8192];
static bool paused,allow_pause=true,interrupted;
static size_t program_limit=ROBOT_CONFIG_RECORD_SIZE;
static unsigned erase_count,program_count,resumes;

bool audio_flash_pause(void) {
    if (!allow_pause) return false;
    assert(!paused);paused=true;return true;
}
void audio_flash_resume(void) {assert(paused);paused=false;resumes++;}
void flash_range_erase(uint32_t offset,size_t count) {
    assert(paused && offset>=ROBOT_CONFIG_FLASH_OFFSET && offset+count<=sizeof(test_flash));
    assert(!(offset%4096) && !(count%4096));
    memset(test_flash+offset,0xff,count);erase_count++;
}
void flash_range_program(uint32_t offset,const uint8_t *data,size_t count) {
    assert(paused && offset>=ROBOT_CONFIG_FLASH_OFFSET && offset+count<=sizeof(test_flash));
    assert(!(offset%256) && !(count%256));
    if (count>program_limit) count=program_limit;
    for (size_t i=0;i<count;i++) test_flash[offset+i]&=data[i];
    program_count++;
}
int flash_safe_execute(void (*callback)(void *),void *context,uint32_t timeout_ms) {
    assert(paused && timeout_ms);callback(context);return interrupted?-1:0;
}
int main(void) {
    memset(test_flash,0xff,sizeof(test_flash));
    RobotConfig config={0},loaded={0};robot_init(&config.robot);
    config.volume=200;config.autoload=true;
    assert(robot_pronounce(&config.robot,"operational","opperational"));
    assert(!storage_load(&loaded));assert(!storage_present());
    assert(storage_save(&config)==1 && storage_present());assert(!paused);
    assert(erase_count==1 && program_count==1 && resumes==1);
    assert(storage_load(&loaded) && loaded.volume==200 && loaded.autoload);
    assert(loaded.robot.rule_count==1);
    assert(storage_save(&config)==0);assert(erase_count==1 && program_count==1);
    config.volume=400;assert(storage_save(&config)==1);
    assert(storage_load(&loaded) && loaded.volume==400);
    memcpy(backup,test_flash+ROBOT_CONFIG_FLASH_OFFSET,sizeof(backup));
    config.volume=300;config.autoload=false;
    interrupted=true;
    for (program_limit=0;program_limit<=ROBOT_CONFIG_RECORD_SIZE;program_limit++) {
        memcpy(test_flash+ROBOT_CONFIG_FLASH_OFFSET,backup,sizeof(backup));
        assert(storage_save(&config)== -1);assert(!paused);
        assert(storage_load(&loaded));
        assert(loaded.volume==(program_limit==ROBOT_CONFIG_RECORD_SIZE?300:400));
    }
    interrupted=false;program_limit=ROBOT_CONFIG_RECORD_SIZE;
    assert(storage_load(&loaded) && !loaded.autoload);
    /* Busy/unsafe audio must prevent writes and leave the saved data intact. */
    allow_pause=false;config.volume=100;
    unsigned before=erase_count;
    assert(storage_save(&config)== -1);assert(!storage_erase());
    assert(erase_count==before);assert(storage_load(&loaded) && loaded.volume==300);
    allow_pause=true;assert(storage_erase());assert(!storage_present());assert(!storage_load(&loaded));
    for (unsigned i=0;i<8192;i++) assert(test_flash[ROBOT_CONFIG_FLASH_OFFSET+i]==0xff);
    /* Save defaults after erase and recover them without a process-side cache. */
    config.volume=100;assert(storage_save(&config)==1);
    assert(storage_load(&loaded) && loaded.volume==100);
    puts("Storage adapter checks passed: real save/load/erase, no-op wear avoidance, audio pause, 1025 interrupted programs, recovery, unsafe-write rejection.");
}
