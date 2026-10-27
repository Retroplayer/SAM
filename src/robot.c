#include "robot.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool eq(const char *a, const char *b) {
    while (*a && *b) {
        if (toupper((unsigned char)*a++) != toupper((unsigned char)*b++)) return false;
    }
    return *a == *b;
}
static uint8_t bound(int n, int lo, int hi) {
    return (uint8_t)(n < lo ? lo : n > hi ? hi : n);
}
bool robot_character(Robot *r, const char *name) {
    if (eq(name, "CLASSIC"))
        r->voice = (RobotVoice){64,72,128,128,0,0,0,false};
    else if (eq(name, "COMPANION"))
        r->voice = (RobotVoice){58,70,145,135,65,0,0,false};
    else if (eq(name, "GUARDIAN"))
        r->voice = (RobotVoice){70,78,135,155,40,10,5,false};
    else if (eq(name, "ENFORCER"))
        r->voice = (RobotVoice){82,84,120,175,30,30,15,false};
    else if (eq(name, "DICTATOR"))
        r->voice = (RobotVoice){94,90,110,185,45,45,20,false};
    else if (eq(name, "MALFUNCTION"))
        r->voice = (RobotVoice){100,96,100,195,80,70,65,true};
    else return false;
    return true;
}
void robot_init(Robot *r) {
    memset(r, 0, sizeof(*r));
    (void)robot_character(r, "COMPANION");
}
bool robot_set(Robot *r, const char *key, unsigned v) {
    uint8_t *field = NULL;
    unsigned max = 255, min = 0;
    if (eq(key,"PITCH")) field=&r->voice.pitch;
    else if (eq(key,"SPEED")) field=&r->voice.speed;
    else if (eq(key,"MOUTH")) field=&r->voice.mouth;
    else if (eq(key,"THROAT")) field=&r->voice.throat;
    else if (eq(key,"EXPRESSION")) {field=&r->voice.expression; max=100;}
    else if (eq(key,"METALLIC")) {field=&r->voice.metallic; max=100;}
    else if (eq(key,"ROUGHNESS")) {field=&r->voice.roughness; max=100;}
    else if (eq(key,"SING")) {
        if (v>1) return false;
        r->voice.sing = v != 0; return true;
    } else return false;
    if (v<min || v>max) return false;
    *field=(uint8_t)v; return true;
}
bool robot_pronounce(Robot *r, const char *term, const char *spoken) {
    size_t n=strlen(term), m=strlen(spoken), i;
    if (!n || n>=sizeof(r->rules[0].term) || !m || m>=sizeof(r->rules[0].spoken)) return false;
    for (i=0;i<n;i++) if (!isalnum((unsigned char)term[i]) && term[i]!='-') return false;
    for (i=0;i<m;i++) if (!isalpha((unsigned char)spoken[i]) && spoken[i]!=' ' && spoken[i]!='\'') return false;
    for (i=0;i<r->rule_count;i++) if (eq(term,r->rules[i].term)) break;
    if (i==ROBOT_RULES_MAX) return false;
    strcpy(r->rules[i].term,term); strcpy(r->rules[i].spoken,spoken);
    if (i==r->rule_count) r->rule_count++;
    return true;
}

typedef struct {
    char raw[32];
    char spoken[80];
    unsigned phrase;
    bool emphasis;
} Word;
typedef struct { char end; uint16_t pause; unsigned bangs; } Phrase;
static const struct { const char *term, *spoken; } dictionary[] = {
    {"CPU","see pee you"},{"USB","you ess bee"},{"AI","ay eye"},
    {"I2C","eye squared see"},{"SPI","ess pee eye"},{"UART","you art"},
    {"LED","ell ee dee"},{"NASA","nasa"},{"RADAR","radar"},
    {"LASER","laser"},{"RAM","ram"},{"ROM","rom"},{"OK","okay"},
    {"DR","doctor"},{"MR","mister"},{"MRS","missus"},{"APPROX","approximately"}
};
static const char *letters[] = {"ay","bee","see","dee","ee","eff","gee","aitch","eye","jay","kay","ell","em","en","oh","pee","cue","ar","ess","tee","you","vee","double you","ex","why","zee"};
static const char *digits[] = {"zero","one","two","three","four","five","six","seven","eight","nine"};
static bool append(char *dst, size_t cap, const char *s) {
    size_t n=strlen(dst), m=strlen(s);
    if (n+m+(n!=0)>=cap) return false;
    if (n) dst[n++]=' ';
    memcpy(dst+n,s,m+1); return true;
}
static bool number_words(char *dst, size_t cap, unsigned value) {
    const char *small[]={"zero","one","two","three","four","five","six","seven","eight","nine","ten","eleven","twelve","thirteen","fourteen","fifteen","sixteen","seventeen","eighteen","nineteen"};
    const char *tens[]={"","","twenty","thirty","forty","fifty","sixty","seventy","eighty","ninety"};
    if (value>=1000) {
        if (!append(dst,cap,small[value/1000]) || !append(dst,cap,"thousand")) return false;
        value%=1000; if (!value) return true;
    }
    if (value>=100) {
        if (!append(dst,cap,small[value/100]) || !append(dst,cap,"hundred")) return false;
        value%=100; if (!value) return true;
    }
    if (value<20) return append(dst,cap,small[value]);
    if (!append(dst,cap,tens[value/10])) return false;
    return !(value%10) || append(dst,cap,small[value%10]);
}
static bool expand(const Robot *r, Word *w, bool all_caps, const char *previous) {
    const char *raw=w->raw;
    unsigned alpha=0, number=0; bool upper=true;
    for (size_t i=0;raw[i];i++) {
        if (isalpha((unsigned char)raw[i])) {alpha++; if (islower((unsigned char)raw[i])) upper=false;}
        if (isdigit((unsigned char)raw[i])) number++;
    }
    for (unsigned i=0;i<r->rule_count;i++) if (eq(raw,r->rules[i].term)) {
        strcpy(w->spoken,r->rules[i].spoken); return true;
    }
    if (previous && isdigit((unsigned char)previous[0])) {
        const char *unit = eq(raw,"V") ? "volts" : eq(raw,"HZ") ? "hertz" : eq(raw,"MA") ? "milliamps" : NULL;
        if (unit) {strcpy(w->spoken,unit); return true;}
    }
    for (size_t i=0;i<sizeof(dictionary)/sizeof(dictionary[0]);i++) if (eq(raw,dictionary[i].term)) {
        strcpy(w->spoken,dictionary[i].spoken); return true;
    }
    /* Unknown short uppercase words are spelled only in mixed-case messages.
       Longer uppercase words receive emphasis rather than acronym treatment. */
    const char *ordinary[]={"WHO","WHAT","WHEN","WHERE","WHY","HOW","WHICH","WHOSE",
        "THE","THIS","THAT","YOU","YOUR","MY","ME","WE","OUR","THEY","IT","ITS",
        "ARE","IS","AM","WAS","WERE","DO","DID","DOES","CAN","WILL","MUST",
        "NOT","NO","YES","NOW","STOP","WAIT","GO","HERE","THEN","AND","OR","BUT",
        "IF","AT","TO","IN","ON","OF","FOR","WITH","BE","HAVE","HAS","ALL"};
    bool known_word=false;
    for (size_t i=0;i<sizeof(ordinary)/sizeof(ordinary[0]);i++) if (eq(raw,ordinary[i])) known_word=true;
    bool numeric=strspn(raw,"0123456789.")==strlen(raw) && strchr(raw,'.')==strrchr(raw,'.');
    bool spell = (alpha && number) || (number && !alpha && !numeric) ||
        (upper && alpha>=2 && alpha<=4 && !all_caps && !known_word);
    if (spell) {
        for (size_t i=0;raw[i];i++) {
            const unsigned char c=(unsigned char)toupper((unsigned char)raw[i]);
            if (isalpha(c) && !append(w->spoken,sizeof(w->spoken),letters[c-'A'])) return false;
            if (isdigit(c) && !append(w->spoken,sizeof(w->spoken),digits[c-'0'])) return false;
        }
    } else if (number && !alpha) {
        size_t whole=strcspn(raw,".");
        if (whole<=4 && (raw[0]!='0' || whole==1)) {
            unsigned value=0;
            for (size_t i=0;i<whole;i++) value=value*10u+(unsigned)(raw[i]-'0');
            if (!number_words(w->spoken,sizeof(w->spoken),value)) return false;
        } else {
            for (size_t i=0;i<whole;i++) if (!append(w->spoken,sizeof(w->spoken),digits[raw[i]-'0'])) return false;
        }
        if (raw[whole]=='.') {
            if (!append(w->spoken,sizeof(w->spoken),"point")) return false;
            for (size_t i=whole+1;raw[i];i++) if (!append(w->spoken,sizeof(w->spoken),digits[raw[i]-'0'])) return false;
        }
    } else {
        strcpy(w->spoken,raw);
        w->emphasis=upper && alpha>1 && !all_caps;
    }
    return true;
}
static bool wh_question(const char *word) {
    const char *wh[]={"WHO","WHAT","WHERE","WHEN","WHY","HOW","WHICH","WHOSE"};
    for (unsigned i=0;i<sizeof(wh)/sizeof(wh[0]);i++) if (eq(word,wh[i])) return true;
    return false;
}

const char *robot_plan(const Robot *r, const char *text, bool phonetic, RobotPlan *out) {
    Word words[ROBOT_WORDS_MAX]={0};
    Phrase phrases[ROBOT_WORDS_MAX]={0};
    size_t n=strlen(text), count=0; unsigned phrase=0;
    out->count=0;
    if (!n || n>ROBOT_TEXT_MAX) return "text must contain 1..255 bytes";
    for (size_t i=0;i<n;i++) if ((unsigned char)text[i]>126 || ((unsigned char)text[i]<32 && text[i]!='\t')) return "ASCII text required";
    if (phonetic) {
        if (n>=sizeof(out->segments[0].text)) return "phoneme input must be under 96 bytes";
        RobotSegment *s=&out->segments[0]; memset(s,0,sizeof(*s));
        strcpy(s->text,text); s->voice=r->voice; s->phonetic=true; out->count=1; return NULL;
    }
    bool all_caps=true;
    for (size_t i=0;i<n;i++) if (islower((unsigned char)text[i])) all_caps=false;
    for (size_t i=0;i<n;) {
        unsigned char c=(unsigned char)text[i];
        if (isalnum(c)) {
            if (count==ROBOT_WORDS_MAX) return "too many words";
            Word *w=&words[count++]; w->phrase=phrase; size_t k=0;
            do {
                if (k+1==sizeof(w->raw)) return "word too long";
                w->raw[k++]=text[i++];
            } while (i<n && (isalnum((unsigned char)text[i]) ||
                ((text[i]=='\'' || text[i]=='-') && i+1<n && isalnum((unsigned char)text[i+1])) ||
                (text[i]=='.' && isdigit((unsigned char)text[i-1]) && i+1<n && isdigit((unsigned char)text[i+1]))));
        } else if (c=='.' || c=='?' || c=='!' || c==',' || c==':' || c==';') {
            Phrase *p=&phrases[phrase];
            if (c=='!') p->bangs++;
            if (c=='?' || (p->end!='?' && c!='!') || !p->end) p->end=(char)c;
            unsigned dots=0;
            if (c=='.') {while (i+1<n && text[i+1]=='.') {dots++;i++;}}
            p->pause=(uint16_t)(dots ? 450 : c==',' ? 100 : c==':' || c==';' ? 180 : 240);
            /* Keep repeated punctuation attached to the preceding phrase. */
            size_t j=i+1;
            while (j<n && (isspace((unsigned char)text[j]) || strchr(".!?,:;",text[j]))) j++;
            bool next_is_punctuation=i+1<n && strchr(".!?,:;",text[i+1]);
            bool abbreviation=c=='.' && count && (eq(words[count-1].raw,"DR") || eq(words[count-1].raw,"MR") || eq(words[count-1].raw,"MRS") || eq(words[count-1].raw,"APPROX")) && j<n;
            if (abbreviation) memset(p,0,sizeof(*p));
            else if (!next_is_punctuation && j<n) {
                if (phrase+1==ROBOT_WORDS_MAX) return "too many phrases";
                phrase++;
            }
            i++;
        } else if (isspace(c) || c=='"' || c=='(' || c==')') i++;
        else return "unsupported text symbol";
    }
    if (!count) return "no speakable words";
    for (size_t i=0;i<count;i++) {
        Word *w=&words[i];
        if (!expand(r,w,all_caps,i?words[i-1].raw:NULL)) {out->count=0;return "pronunciation expansion too long";}
        size_t first=i,last=i;
        while (first>0 && words[first-1].phrase==w->phrase) first--;
        while (last+1<count && words[last+1].phrase==w->phrase) last++;
        Phrase *p=&phrases[w->phrase]; RobotVoice v=r->voice;
        int e=v.expression, delta=0;
        /* SAM pitch is an inverse period: a smaller value sounds higher. */
        if (i==last) {
            bool rising=p->end=='?' && !wh_question(words[first].raw);
            delta=(rising ? -1 : 1)*(e/8);
        } else if (i==first && eq(w->raw,"HELLO")) delta=-e/12;
        if (p->bangs) delta-=e/14;
        if (w->emphasis) delta-=e/6;
        v.pitch=bound((int)v.pitch+delta,0,255);
        if (w->emphasis) v.speed=bound((int)v.speed+e/10,0,255);
        RobotSegment *s=out->count?&out->segments[out->count-1]:NULL;
        bool merge=s && !s->pause_ms && s->voice.pitch==v.pitch && s->voice.speed==v.speed;
        if (merge && strlen(s->text)+strlen(w->spoken)+1>=sizeof(s->text)) merge=false;
        if (!merge) {
            if (out->count==ROBOT_SEGMENTS_MAX) {out->count=0; return "too many speech segments";}
            s=&out->segments[out->count++]; memset(s,0,sizeof(*s)); s->voice=v;
        }
        if (!append(s->text,sizeof(s->text),w->spoken)) {out->count=0;return "speech segment too long";}
        if (i==last) s->pause_ms=p->pause;
    }
    return NULL;
}
void robot_plan_print(const RobotPlan *plan) {
    for (size_t i=0;i<plan->count;i++) {
        const RobotSegment *s=&plan->segments[i];
        printf("%02zu pitch=%u speed=%u mouth=%u throat=%u sing=%u metal=%u rough=%u pause=%ums %s%s\n",
            i,s->voice.pitch,s->voice.speed,s->voice.mouth,s->voice.throat,s->voice.sing,
            s->voice.metallic,s->voice.roughness,s->pause_ms,s->phonetic?"PHONEMES ":"",s->text);
    }
}
