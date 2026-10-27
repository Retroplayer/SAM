#include "robot.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { FILE *file; uint32_t count; } Wave;
static void little(FILE *f, uint32_t value, unsigned bytes) {
    while (bytes--) {fputc((int)(value&255),f);value>>=8;}
}
static bool sample(void *context, uint8_t value) {
    Wave *w=context;
    if (w->count==UINT32_MAX-36) return false;
    if (fputc(value,w->file)==EOF) return false;
    w->count++; return true;
}
static bool speak(const RobotPlan *p, const char *path) {
    Wave w={fopen(path,"wb"),0};
    if (!w.file) {perror(path);return false;}
    fwrite("RIFF",1,4,w.file); little(w.file,0,4); fwrite("WAVEfmt ",1,8,w.file);
    little(w.file,16,4);little(w.file,1,2);little(w.file,1,2);
    little(w.file,ROBOT_RATE,4);little(w.file,ROBOT_RATE,4);little(w.file,1,2);little(w.file,8,2);
    fwrite("data",1,4,w.file);little(w.file,0,4);
    bool ok=robot_render(p,sample,&w);
    if (w.count&1u) fputc(0,w.file);
    if (ferror(w.file) || fseek(w.file,4,SEEK_SET)) ok=false;
    little(w.file,36+w.count+(w.count&1u),4);
    if (fseek(w.file,40,SEEK_SET)) ok=false;
    little(w.file,w.count,4);
    if (fclose(w.file)) ok=false;
    if (!ok) {remove(path);fprintf(stderr,"ERR synthesis or WAV write failed\n");return false;}
    printf("DONE %s samples=%u\n",path,w.count);return true;
}
static bool plan_say(Robot *r, const char *text, bool phonetic, bool inspect, const char *path) {
    RobotPlan plan;
    const char *error=robot_plan(r,text,phonetic,&plan);
    if (error) {fprintf(stderr,"ERR %s\n",error);return false;}
    if (inspect) {robot_plan_print(&plan);return true;}
    return speak(&plan,path);
}
static void help(void) {
    puts("robot-sam [--character NAME] [--plan] [--phonemes] [--wav FILE] TEXT\n"
         "robot-sam --console [--plan]\n\n"
         "Console commands (case sensitive):\n"
         "  CHARACTER COMPANION|GUARDIAN|ENFORCER|DICTATOR|MALFUNCTION|CLASSIC\n"
         "  SET PITCH|SPEED|MOUTH|THROAT|EXPRESSION|METALLIC|ROUGHNESS|SING number\n"
         "  PRONOUNCE term=spoken words\n"
         "  SAVE_CHARACTER file\n  LOAD_CHARACTER file\n"
         "  SAY text\n  PHONEMES SAM phonemes\n  STATUS\n  HELP\n  QUIT\n"
         "Each SAY writes speech-NNN.wav in the current directory.\n"
         "SAVE_CHARACTER persists voice and pronunciation settings to a file.");
}
static int console(Robot *r, bool inspect) {
    char line[512]; unsigned sequence=0; bool failed=false;
    while (fgets(line,sizeof(line),stdin)) {
        size_t n=strlen(line);
        if (n && line[n-1]!='\n' && !feof(stdin)) {
            int c; while ((c=getchar())!='\n' && c!=EOF) {}
            fprintf(stderr,"ERR command too long\n");failed=true;continue;
        }
        line[strcspn(line,"\r\n")]=0;
        if (!*line || *line=='#') continue;
        bool ok=true;
        if (!strcmp(line,"QUIT")) break;
        else if (!strcmp(line,"HELP")) help();
        else if (!strncmp(line,"SAVE_CHARACTER ",15)) {
            ok=robot_profile_save(r,line+15); if (ok) puts("OK");
        } else if (!strncmp(line,"LOAD_CHARACTER ",15)) {
            ok=robot_profile_load(r,line+15); if (ok) puts("OK");
        }
        else if (!strcmp(line,"STATUS")) printf("IDLE pitch=%u speed=%u mouth=%u throat=%u expression=%u metallic=%u roughness=%u sing=%u rules=%u\n",r->voice.pitch,r->voice.speed,r->voice.mouth,r->voice.throat,r->voice.expression,r->voice.metallic,r->voice.roughness,r->voice.sing,r->rule_count);
        else if (!strncmp(line,"CHARACTER ",10)) {
            ok=robot_character(r,line+10); if (ok) puts("OK");
        } else if (!strncmp(line,"SET ",4)) {
            char key[24],value[24],extra; unsigned long v; char *end;
            ok=sscanf(line+4,"%23s %23s %c",key,value,&extra)==2;
            if (ok) {errno=0;v=strtoul(value,&end,10);ok=!errno && !*end && value[0]!='-' && v<=255 && robot_set(r,key,(unsigned)v);}
            if (ok) puts("OK");
        } else if (!strncmp(line,"PRONOUNCE ",10)) {
            char *sep=strchr(line+10,'=');ok=sep!=NULL;
            if (ok) {*sep=0;ok=robot_pronounce(r,line+10,sep+1);}
            if (ok) puts("OK");
        } else if (!strncmp(line,"SAY ",4) || !strncmp(line,"PHONEMES ",9)) {
            bool phonetic=line[0]=='P';char path[64];
            snprintf(path,sizeof(path),"speech-%03u.wav",++sequence);
            ok=plan_say(r,line+(phonetic?9:4),phonetic,inspect,path);
            if (!ok) {failed=true;continue;}
        } else ok=false;
        if (!ok) {fprintf(stderr,"ERR invalid command or value\n");failed=true;}
    }
    return failed || ferror(stdin) ? 1 : 0;
}
int main(int argc, char **argv) {
    Robot robot; robot_init(&robot);
    bool inspect=false,phonetic=false,interactive=false;
    const char *text=NULL,*path="speech.wav";
    for (int i=1;i<argc;i++) {
        if (!strcmp(argv[i],"--help")) {help();return 0;}
        else if (!strcmp(argv[i],"--console")) interactive=true;
        else if (!strcmp(argv[i],"--plan")) inspect=true;
        else if (!strcmp(argv[i],"--phonemes")) phonetic=true;
        else if ((!strcmp(argv[i],"--character") || !strcmp(argv[i],"--wav")) && i+1<argc) {
            bool character=!strcmp(argv[i],"--character"); const char *v=argv[++i];
            if (character && !robot_character(&robot,v)) {fprintf(stderr,"Unknown character: %s\n",v);return 2;}
            if (!character) path=v;
        } else if (argv[i][0]=='-' || text) {help();return 2;}
        else text=argv[i];
    }
    if (interactive && !text) return console(&robot,inspect);
    if (!text || interactive) {help();return 2;}
    return plan_say(&robot,text,phonetic,inspect,path)?0:1;
}
