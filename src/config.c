#include "robot_config.h"
#include "robot_pcm.h"
#include <string.h>

static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static void put32(uint8_t *p,uint32_t value) {
    for (unsigned i=0;i<4;i++) p[i]=(uint8_t)(value>>(i*8));
}
static uint32_t checksum(const uint8_t *record) {
    uint32_t crc=UINT32_MAX;
    for (unsigned i=0;i<ROBOT_CONFIG_RECORD_SIZE-4;i++) {
        if (i>=12 && i<16) continue;
        crc^=record[i];
        for (unsigned bit=0;bit<8;bit++) crc=(crc>>1)^((crc&1)?0xedb88320u:0);
    }
    return ~crc;
}
bool robot_config_encode(const RobotConfig *c,uint32_t sequence,uint8_t out[ROBOT_CONFIG_RECORD_SIZE]) {
    if (c->volume>ROBOT_VOLUME_MAX || c->robot.rule_count>ROBOT_RULES_MAX) return false;
    memset(out,0xff,ROBOT_CONFIG_RECORD_SIZE);
    memcpy(out,"RSAM",4);put32(out+4,1);put32(out+8,sequence);
    out[16]=(uint8_t)c->volume;out[17]=(uint8_t)(c->volume>>8);
    out[18]=c->autoload;out[19]=c->robot.rule_count;
    const RobotVoice *v=&c->robot.voice;
    const uint8_t fields[]={v->pitch,v->speed,v->mouth,v->throat,
        v->expression,v->metallic,v->roughness,v->sing};
    memcpy(out+20,fields,sizeof(fields));
    for (unsigned i=0;i<c->robot.rule_count;i++) {
        if (!memchr(c->robot.rules[i].term,0,16) || !memchr(c->robot.rules[i].spoken,0,64)) return false;
        uint8_t *rule=out+28+i*80;
        memset(rule,0,80);
        strcpy((char *)rule,c->robot.rules[i].term);
        strcpy((char *)rule+16,c->robot.rules[i].spoken);
    }
    put32(out+12,checksum(out));
    put32(out+ROBOT_CONFIG_RECORD_SIZE-4,0x5a17c0de);
    return robot_config_decode(out,NULL,NULL);
}
bool robot_config_decode(const uint8_t *record,RobotConfig *out,uint32_t *sequence) {
    if (memcmp(record,"RSAM",4) || get32(record+4)!=1 ||
        get32(record+ROBOT_CONFIG_RECORD_SIZE-4)!=0x5a17c0de ||
        get32(record+12)!=checksum(record)) return false;
    RobotConfig candidate;
    memset(&candidate,0,sizeof(candidate));
    candidate.volume=(uint16_t)(record[16]|(unsigned)record[17]<<8);
    if (candidate.volume>ROBOT_VOLUME_MAX || record[18]>1 || record[19]>ROBOT_RULES_MAX || record[27]>1) return false;
    candidate.autoload=record[18]!=0;
    robot_init(&candidate.robot);
    const char *keys[]={"PITCH","SPEED","MOUTH","THROAT","EXPRESSION","METALLIC","ROUGHNESS","SING"};
    for (unsigned i=0;i<8;i++) if (!robot_set(&candidate.robot,keys[i],record[20+i])) return false;
    for (unsigned i=0;i<record[19];i++) {
        const char *term=(const char *)record+28+i*80,*spoken=term+16;
        if (!memchr(term,0,16) || !memchr(spoken,0,64) ||
            !robot_pronounce(&candidate.robot,term,spoken) || candidate.robot.rule_count!=i+1) return false;
    }
    if (out) *out=candidate;
    if (sequence) *sequence=get32(record+8);
    return true;
}
int robot_config_select(const uint8_t *a,const uint8_t *b,RobotConfig *out,uint32_t *sequence) {
    uint32_t first=0,second=0;
    bool valid_a=robot_config_decode(a,NULL,&first),valid_b=robot_config_decode(b,NULL,&second);
    if (!valid_a && !valid_b) return -1;
    uint32_t delta=second-first;
    int slot=valid_b && (!valid_a || (delta && delta<0x80000000u))?1:0;
    if (!robot_config_decode(slot?b:a,out,sequence)) return -1;
    return slot;
}
