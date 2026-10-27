#ifndef ROBOT_H
#define ROBOT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ROBOT_RATE 22050u
#define ROBOT_TEXT_MAX 255
#define ROBOT_WORDS_MAX 40
#define ROBOT_SEGMENTS_MAX 40
#define ROBOT_RULES_MAX 12

typedef struct {
    uint8_t pitch, speed, mouth, throat;
    uint8_t expression, metallic, roughness;
    bool sing;
} RobotVoice;
typedef struct { char term[16], spoken[64]; } RobotRule;
typedef struct {
    RobotVoice voice;
    RobotRule rules[ROBOT_RULES_MAX];
    uint8_t rule_count;
} Robot;
typedef struct {
    char text[96];
    RobotVoice voice;
    uint16_t pause_ms;
    bool phonetic;
} RobotSegment;
typedef struct {
    RobotSegment segments[ROBOT_SEGMENTS_MAX];
    size_t count;
} RobotPlan;

void robot_init(Robot *robot);
bool robot_character(Robot *robot, const char *name);
bool robot_set(Robot *robot, const char *key, unsigned value);
bool robot_pronounce(Robot *robot, const char *term, const char *spoken);
/* Planning is atomic: count is zero on error. ASCII input, no heap allocation. */
const char *robot_plan(const Robot *robot, const char *text, bool phonetic,
                       RobotPlan *plan);
typedef bool (*RobotSink)(void *context, uint8_t sample);
/* Single synthesis instance; callback backpressure is supported. */
bool robot_render(const RobotPlan *plan, RobotSink sink, void *context);
void robot_plan_print(const RobotPlan *plan);
/* Desktop persistence adapter; embedded flash storage can replace this. */
bool robot_profile_save(const Robot *robot, const char *path);
bool robot_profile_load(Robot *robot, const char *path);
#endif
