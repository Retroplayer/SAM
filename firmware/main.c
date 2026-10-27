#include "audio.h"
#include "robot.h"
#include "hardware/uart.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico/util/queue.h"
#include "pico/sem.h"
#include <errno.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Robot robot;
static RobotPlan plan;
static Robot job_robot;
static char job_text[ROBOT_TEXT_MAX+1];
static bool job_phonetic;
static const char *job_error;
/* Planning and reference synthesis need more than the RP2040 scratch stack. */
static uint32_t worker_stack[4096];
static semaphore_t work;
static queue_t events;
static atomic_bool busy;
enum { EVENT_STARTED, EVENT_DONE, EVENT_STOPPED, EVENT_ERROR };

static void reply(const char *format, ...) {
    char text[384]; va_list args;
    va_start(args,format);vsnprintf(text,sizeof(text),format,args);va_end(args);
    puts(text);
    uart_puts(uart0,text);uart_puts(uart0,"\r\n");
}
static void worker(void) {
    for (;;) {
        sem_acquire_blocking(&work);
        /* begin is called by core 0 before accepting STOP to avoid clearing it. */
        unsigned event;
        job_error=robot_plan(&job_robot,job_text,job_phonetic,&plan);
        bool ok=false;
        if (!job_error) {
            event=EVENT_STARTED;queue_add_blocking(&events,&event);
            ok=robot_render(&plan,audio_sample,NULL);
        }
        audio_finish();
        event=audio_stopped()?EVENT_STOPPED:ok?EVENT_DONE:EVENT_ERROR;
        queue_add_blocking(&events,&event);
        /* Core 0 clears busy after consuming this event, preserving event order. */
    }
}

static void command(char *line) {
    if (!*line) return;
    if (!strcmp(line,"STATUS")) {
        reply("%s audio=%s underruns=%lu pitch=%u speed=%u mouth=%u throat=%u expression=%u metallic=%u roughness=%u sing=%u rules=%u",
            atomic_load(&busy)?"BUSY":"IDLE",audio_backend(),(unsigned long)audio_underruns(),
            robot.voice.pitch,robot.voice.speed,robot.voice.mouth,robot.voice.throat,
            robot.voice.expression,robot.voice.metallic,robot.voice.roughness,robot.voice.sing,robot.rule_count);
    } else if (!strcmp(line,"HELP")) {
        reply("CHARACTER name | SET key value | PRONOUNCE term=words | SAY text | PHONEMES symbols | STATUS | STOP");
    } else if (!strcmp(line,"STOP")) {
        if (atomic_load(&busy)) {audio_request_stop();reply("OK stopping");}
        else reply("OK idle");
    } else if (!strncmp(line,"SAY ",4) || !strncmp(line,"PHONEMES ",9)) {
        if (atomic_load(&busy)) {reply("ERR busy");return;}
        bool phonetic=line[0]=='P';
        const char *text=line+(phonetic?9:4);
        if (!*text || strlen(text)>ROBOT_TEXT_MAX) {reply("ERR text must contain 1..255 bytes");return;}
        job_robot=robot;strcpy(job_text,text);job_phonetic=phonetic;
        audio_begin();
        atomic_store(&busy,true);
        reply("ACCEPTED");sem_release(&work);
    } else if (!strncmp(line,"CHARACTER ",10)) {
        reply(robot_character(&robot,line+10)?"OK":"ERR unknown character");
    } else if (!strncmp(line,"SET ",4)) {
        char key[24],value[24],extra,*end; unsigned long v;
        bool ok=sscanf(line+4,"%23s %23s %c",key,value,&extra)==2;
        if (ok) {errno=0;v=strtoul(value,&end,10);ok=!errno && !*end && value[0]!='-' && v<=255 && robot_set(&robot,key,(unsigned)v);}
        reply(ok?"OK":"ERR invalid setting");
    } else if (!strncmp(line,"PRONOUNCE ",10)) {
        char *separator=strchr(line+10,'=');bool ok=separator!=NULL;
        if (ok) {*separator=0;ok=robot_pronounce(&robot,line+10,separator+1);}
        reply(ok?"OK":"ERR invalid pronunciation");
    } else reply("ERR unknown command");
}

typedef struct {char data[ROBOT_TEXT_MAX+16];size_t length;bool overflow;} Line;
static void receive(Line *line, int c) {
    if (c=='\r' || c=='\n') {
        if (line->overflow) reply("ERR command too long");
        else {line->data[line->length]=0;command(line->data);}
        line->length=0;line->overflow=false;
    } else if (c>=32 && c<=126) {
        if (line->length+1<sizeof(line->data)) line->data[line->length++]=(char)c;
        else line->overflow=true;
    } else if (c=='\t') {
        if (line->length+1<sizeof(line->data)) line->data[line->length++]=' ';
        else line->overflow=true;
    } else line->overflow=true;
}
int main(void) {
    stdio_init_all();
    uart_init(uart0,115200);
    gpio_set_function(ROBOT_UART_TX_PIN,GPIO_FUNC_UART);
    gpio_set_function(ROBOT_UART_RX_PIN,GPIO_FUNC_UART);
    uart_set_format(uart0,8,1,UART_PARITY_NONE);
    uart_set_hw_flow(uart0,false,false);
    robot_init(&robot);
    sem_init(&work,0,1);queue_init(&events,sizeof(unsigned),8);
    audio_init();multicore_launch_core1_with_stack(worker,worker_stack,sizeof(worker_stack));
    reply("READY robot-sam 0.2.0 audio=%s rate=%u",audio_backend(),ROBOT_RATE);
    Line usb={0},serial={0};
    for (;;) {
        unsigned event;
        while (queue_try_remove(&events,&event)) {
            if (event==EVENT_STARTED) reply("STARTED");
            else {
                atomic_store(&busy,false);
                if (event==EVENT_ERROR) reply("ERR %s",job_error?job_error:"synthesis failed");
                else reply("%s underruns=%lu",event==EVENT_DONE?"DONE":"STOPPED",(unsigned long)audio_underruns());
            }
        }
        /* Bound each poll so one busy input cannot starve the other or events. */
        for (unsigned i=0;i<32 && uart_is_readable(uart0);i++) receive(&serial,uart_getc(uart0));
        for (unsigned i=0;i<32;i++) {
            int c=getchar_timeout_us(0);
            if (c==PICO_ERROR_TIMEOUT) break;
            receive(&usb,c);
        }
        tight_loop_contents();
    }
}
