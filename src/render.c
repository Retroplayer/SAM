#include "robot.h"
#include "SamData.h"
#include "reciter.h"
#include "sam.h"
#include <ctype.h>
#include <string.h>

/* The reference engine is global and non-reentrant. Serialize calls. */
static SamData state;
typedef struct {
    RobotSink sink;
    void *context;
    RobotVoice voice;
    uint32_t phase;
    bool okay;
} Output;

static void emit(void *context, unsigned char sample) {
    Output *o=context;
    if (!o->okay) return;
    int value=(int)sample-128;
    /* Integer square-wave ring modulator, intentionally crude and retro. */
    o->phase+=140;
    if (o->phase>=ROBOT_RATE) o->phase-=ROBOT_RATE;
    int ring=o->phase<ROBOT_RATE/2 ? value : -value;
    value=(value*(100-o->voice.metallic)+ring*o->voice.metallic)/100;
    unsigned drop=o->voice.roughness*5u/100u;
    int unsigned_value=value+128;
    if (unsigned_value<0) unsigned_value=0;
    if (unsigned_value>255) unsigned_value=255;
    /* Quantize around the unsigned PCM midpoint; zero stays silent. */
    unsigned_value &= ~((1u<<drop)-1u);
    o->okay=o->sink(o->context,(uint8_t)unsigned_value);
}

bool robot_render(const RobotPlan *plan, RobotSink sink, void *context) {
    if (!plan || !plan->count || !sink) return false;
    memset(&state,0,sizeof(state)); samdata=&state;
    Output out={.sink=sink,.context=context,.okay=true};
    for (size_t i=0;i<plan->count && out.okay;i++) {
        const RobotSegment *s=&plan->segments[i];
        char input[256]={0}; size_t n=strlen(s->text);
        if (n>95) return false;
        for (size_t j=0;j<n;j++) input[j]=(char)toupper((unsigned char)s->text[j]);
        input[n]=s->phonetic ? (char)0x9b : '[';
        out.voice=s->voice;
        SetPitch(s->voice.pitch); SetSpeed(s->voice.speed);
        SetMouth(s->voice.mouth); SetThroat(s->voice.throat);
        EnableSingmode(s->voice.sing);
        if (!s->phonetic && !TextToPhonemes(input)) return false;
        SetInput(input);
        if (!SAMMain(emit,&out) || !out.okay) return false;
        for (uint32_t j=0;j<(uint32_t)s->pause_ms*ROBOT_RATE/1000u;j++)
            if (!sink(context,128)) return false;
    }
    return out.okay;
}
