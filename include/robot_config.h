#ifndef ROBOT_CONFIG_H
#define ROBOT_CONFIG_H
#include "robot.h"

#define ROBOT_CONFIG_RECORD_SIZE 1024u
typedef struct {
    Robot robot;
    uint16_t volume;
    bool autoload;
} RobotConfig;

bool robot_config_encode(const RobotConfig *config, uint32_t sequence,
                         uint8_t record[ROBOT_CONFIG_RECORD_SIZE]);
/* Invalid records never modify the caller's configuration or sequence. */
bool robot_config_decode(const uint8_t *record, RobotConfig *config, uint32_t *sequence);
/* Return newest valid journal slot, or -1. Sequence rollover is supported. */
int robot_config_select(const uint8_t *a, const uint8_t *b,
                        RobotConfig *config, uint32_t *sequence);
#endif
