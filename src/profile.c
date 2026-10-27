#include "robot.h"
#include <stdio.h>
#include <string.h>

/* Text format independent of struct padding and target endianness. */
bool robot_profile_save(const Robot *r, const char *path) {
    FILE *f=fopen(path,"w");
    if (!f) return false;
    const RobotVoice *v=&r->voice;
    bool ok=fprintf(f,"ROBOT-SAM 1\nVOICE %u %u %u %u %u %u %u %u\n",v->pitch,v->speed,
        v->mouth,v->throat,v->expression,v->metallic,v->roughness,v->sing)>0;
    for (unsigned i=0;i<r->rule_count;i++)
        if (fprintf(f,"RULE %s=%s\n",r->rules[i].term,r->rules[i].spoken)<0) ok=false;
    if (fputs("END\n",f)==EOF) ok=false;
    if (fclose(f)) ok=false;
    return ok;
}
bool robot_profile_load(Robot *r, const char *path) {
    FILE *f=fopen(path,"r");
    if (!f) return false;
    Robot candidate; robot_init(&candidate);
    char line[128], extra; unsigned values[8];
    bool ok=fgets(line,sizeof(line),f) && !strcmp(line,"ROBOT-SAM 1\n");
    if (ok) ok=fgets(line,sizeof(line),f) &&
        sscanf(line,"VOICE %u %u %u %u %u %u %u %u %c",&values[0],&values[1],&values[2],&values[3],&values[4],&values[5],&values[6],&values[7],&extra)==8;
    const char *keys[]={"PITCH","SPEED","MOUTH","THROAT","EXPRESSION","METALLIC","ROUGHNESS","SING"};
    if (ok) for (unsigned i=0;i<8;i++) if (!robot_set(&candidate,keys[i],values[i])) ok=false;
    bool ended=false;
    while (ok && fgets(line,sizeof(line),f)) {
        if (!strcmp(line,"END\n")) {ended=true;break;}
        if (strncmp(line,"RULE ",5) || !strchr(line,'\n')) {ok=false;break;}
        line[strcspn(line,"\n")]=0;
        char *sep=strchr(line+5,'=');
        if (!sep) {ok=false;break;}
        *sep=0;
        if (!robot_pronounce(&candidate,line+5,sep+1)) ok=false;
    }
    if (!ended || ferror(f) || fgetc(f)!=EOF) ok=false;
    if (fclose(f)) ok=false;
    if (ok) *r=candidate;
    return ok;
}
