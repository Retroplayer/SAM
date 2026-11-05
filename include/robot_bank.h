#ifndef ROBOT_BANK_H
#define ROBOT_BANK_H
#include "robot_config.h"
#define ROBOT_CHARACTERS 8
#define ROBOT_DICTIONARY 64
#define ROBOT_BANK_RECORD_SIZE 6144u
typedef struct {
  bool enabled;
  char name[24];
  RobotVoice voice;
} RobotCharacter;
typedef struct {
  RobotCharacter characters[ROBOT_CHARACTERS];
  RobotRule rules[ROBOT_DICTIONARY];
  uint8_t rule_count, startup, framing; /* framing: 0=8N1, 1=7E1 */
  uint16_t volume;
  uint32_t baud;
  bool autoload;
} RobotBank;
void robot_bank_defaults(RobotBank *bank);
void robot_bank_migrate(const RobotConfig *legacy, RobotBank *bank);
bool robot_bank_baud(unsigned baud);
bool robot_bank_name(const char *name);
bool robot_bank_voice_set(RobotVoice *voice, const char *key, unsigned value);
bool robot_bank_rule(RobotBank *bank, const char *term, const char *spoken);
bool robot_bank_delete_rule(RobotBank *bank, const char *term);
bool robot_bank_valid(const RobotBank *bank);
bool robot_bank_encode(const RobotBank *bank, uint32_t sequence, uint8_t *out);
bool robot_bank_decode(const uint8_t *record, RobotBank *bank,
                       uint32_t *sequence);
int robot_bank_select(const uint8_t *a, const uint8_t *b, RobotBank *bank,
                      uint32_t *sequence);
#endif
