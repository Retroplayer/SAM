#include "robot_config.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t a[ROBOT_CONFIG_RECORD_SIZE],b[ROBOT_CONFIG_RECORD_SIZE],scratch[ROBOT_CONFIG_RECORD_SIZE];
static void check(const RobotConfig *expected,const RobotConfig *actual) {
    assert(expected->volume==actual->volume && expected->autoload==actual->autoload);
    assert(!memcmp(&expected->robot.voice,&actual->robot.voice,sizeof(RobotVoice)));
    assert(expected->robot.rule_count==actual->robot.rule_count);
    for (unsigned i=0;i<expected->robot.rule_count;i++) {
        assert(!strcmp(expected->robot.rules[i].term,actual->robot.rules[i].term));
        assert(!strcmp(expected->robot.rules[i].spoken,actual->robot.rules[i].spoken));
    }
}
int main(void) {
    RobotConfig config={0},decoded={0},sentinel={0};uint32_t sequence;
    robot_init(&config.robot);config.volume=200;config.autoload=true;
    assert(robot_character(&config.robot,"CLASSIC"));assert(robot_set(&config.robot,"PITCH",93));
    assert(robot_pronounce(&config.robot,"operational","opperational"));
    assert(robot_pronounce(&config.robot,"identify","I dentify"));
    assert(robot_pronounce(&config.robot,"immediately","eemeedeiatly"));
    assert(robot_config_encode(&config,1,a));
    assert(robot_config_decode(a,&decoded,&sequence) && sequence==1);check(&config,&decoded);
    assert(robot_config_encode(&config,1,scratch));assert(!memcmp(a,scratch,sizeof(a)));
    memset(b,0xff,sizeof(b));
    assert(robot_config_select(b,b,&decoded,NULL)== -1);
    assert(robot_config_select(a,b,&decoded,&sequence)==0 && sequence==1);
    config.volume=400;config.autoload=false;
    assert(robot_config_encode(&config,2,b));
    assert(robot_config_select(a,b,&decoded,&sequence)==1 && sequence==2);check(&config,&decoded);

    /* Every prefix interruption during programming the inactive slot preserves
       the old slot, until the entire new record including commit marker exists. */
    for (unsigned cut=0;cut<=sizeof(b);cut++) {
        memset(scratch,0xff,sizeof(scratch));memcpy(scratch,b,cut);
        int selected=robot_config_select(a,scratch,&decoded,&sequence);
        assert(selected==(cut==sizeof(b)?1:0));
        assert(sequence==(cut==sizeof(b)?2u:1u));
    }
    /* CRC covers metadata, payload, and padding; marker checked independently. */
    for (unsigned offset=0;offset<sizeof(b);offset++) {
        memcpy(scratch,b,sizeof(b));scratch[offset]^=1;
        decoded=sentinel;sequence=123;
        assert(!robot_config_decode(scratch,&decoded,&sequence));
        assert(!memcmp(&decoded,&sentinel,sizeof(decoded)) && sequence==123);
        assert(robot_config_select(a,scratch,NULL,NULL)==0);
    }
    assert(robot_config_encode(&config,UINT32_MAX,a));
    assert(robot_config_encode(&config,0,b));
    assert(robot_config_select(a,b,&decoded,&sequence)==1 && sequence==0);
    for (unsigned i=config.robot.rule_count;i<ROBOT_RULES_MAX;i++) {
        char term[16];snprintf(term,sizeof(term),"TERM%u",i);
        assert(robot_pronounce(&config.robot,term,"test word"));
    }
    assert(robot_config_encode(&config,3,a));assert(robot_config_decode(a,&decoded,NULL));check(&config,&decoded);
    config.volume=401;assert(!robot_config_encode(&config,4,a));config.volume=100;
    config.robot.voice.expression=101;assert(!robot_config_encode(&config,4,a));
    config.robot.voice.expression=0;config.robot.rule_count=ROBOT_RULES_MAX+1;
    assert(!robot_config_encode(&config,4,a));config.robot.rule_count=1;
    memset(config.robot.rules[0].term,'x',sizeof(config.robot.rules[0].term));
    assert(!robot_config_encode(&config,4,a));
    puts("Config checks passed: full round trip, volume/rules/boot policy, both slots, 1025 interrupted writes, 1024 corruptions, sequence rollover, bounds.");
}
