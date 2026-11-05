#include "robot_bank.h"
#include "robot_pcm.h"
#include <string.h>

static bool same(const char *a, const char *b) {
  while (*a && *b) {
    char x = *a++, y = *b++;
    if (x >= 'a' && x <= 'z')
      x -= 32;
    if (y >= 'a' && y <= 'z')
      y -= 32;
    if (x != y)
      return false;
  }
  return *a == *b;
}
static bool alpha(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}
bool robot_bank_name(const char *name) {
  size_t n = 0;
  while (*name) {
    char c = *name++;
    if (++n > 23 ||
        (!alpha(c) && !(c >= '0' && c <= '9') && c != '_' && c != '-'))
      return false;
  }
  return n != 0;
}
static bool rule_valid(const char *term, const char *spoken) {
  size_t n = 0;
  while (*term) {
    char c = *term++;
    if (++n > 15 || (!alpha(c) && !(c >= '0' && c <= '9') && c != '-'))
      return false;
  }
  if (!n)
    return false;
  n = 0;
  bool letter = false;
  while (*spoken) {
    char c = *spoken++;
    if (++n > 63 || (!alpha(c) && c != ' ' && c != '\''))
      return false;
    if (alpha(c))
      letter = true;
  }
  return letter;
}
bool robot_bank_baud(unsigned baud) {
  const unsigned rates[] = {1200,  2400,  4800,  9600,
                            19200, 38400, 57600, 115200};
  for (unsigned i = 0; i < 8; i++)
    if (baud == rates[i])
      return true;
  return false;
}
static bool voice_valid(const RobotVoice *v) {
  return v->expression <= 100 && v->metallic <= 100 && v->roughness <= 100;
}
bool robot_bank_voice_set(RobotVoice *voice, const char *key, unsigned value) {
  Robot r = {0};
  r.voice = *voice;
  if (!robot_set(&r, key, value))
    return false;
  *voice = r.voice;
  return true;
}
void robot_bank_defaults(RobotBank *b) {
  memset(b, 0, sizeof(*b));
  b->volume = ROBOT_VOLUME_DEFAULT;
  b->baud = 9600;
  b->autoload = true;
  b->startup = 1;
  const char *names[] = {"CLASSIC",  "COMPANION", "GUARDIAN",
                         "ENFORCER", "DICTATOR",  "MALFUNCTION"};
  Robot r;
  for (unsigned i = 0; i < 6; i++) {
    robot_init(&r);
    robot_character(&r, names[i]);
    b->characters[i].enabled = true;
    strcpy(b->characters[i].name, names[i]);
    b->characters[i].voice = r.voice;
  }
}
void robot_bank_migrate(const RobotConfig *c, RobotBank *b) {
  robot_bank_defaults(b);
  b->volume = c->volume;
  b->autoload = c->autoload;
  b->baud = 115200;
  strcpy(b->characters[1].name, "ROBOT");
  b->characters[1].voice = c->robot.voice;
  b->rule_count = c->robot.rule_count;
  memcpy(b->rules, c->robot.rules, b->rule_count * sizeof(RobotRule));
}
bool robot_bank_rule(RobotBank *b, const char *term, const char *spoken) {
  if (!rule_valid(term, spoken))
    return false;
  unsigned i;
  for (i = 0; i < b->rule_count; i++)
    if (same(term, b->rules[i].term))
      break;
  if (i >= ROBOT_DICTIONARY)
    return false;
  memset(&b->rules[i], 0, sizeof(RobotRule));
  strcpy(b->rules[i].term, term);
  strcpy(b->rules[i].spoken, spoken);
  if (i == b->rule_count)
    b->rule_count++;
  return true;
}
bool robot_bank_delete_rule(RobotBank *b, const char *term) {
  for (unsigned i = 0; i < b->rule_count; i++)
    if (same(term, b->rules[i].term)) {
      memmove(&b->rules[i], &b->rules[i + 1],
              (b->rule_count - i - 1) * sizeof(RobotRule));
      memset(&b->rules[--b->rule_count], 0, sizeof(RobotRule));
      return true;
    }
  return false;
}
bool robot_bank_valid(const RobotBank *b) {
  if (b->volume > 400 || !robot_bank_baud(b->baud) || b->framing > 1 ||
      b->startup >= 8 || b->rule_count > 64 ||
      !b->characters[b->startup].enabled)
    return false;
  for (unsigned i = 0; i < 8; i++)
    if (b->characters[i].enabled) {
      if (!memchr(b->characters[i].name, 0, 24) ||
          !robot_bank_name(b->characters[i].name) ||
          !voice_valid(&b->characters[i].voice))
        return false;
    }
  for (unsigned i = 0; i < b->rule_count; i++) {
    if (!memchr(b->rules[i].term, 0, 16) ||
        !memchr(b->rules[i].spoken, 0, 64) ||
        !rule_valid(b->rules[i].term, b->rules[i].spoken))
      return false;
    for (unsigned j = 0; j < i; j++)
      if (same(b->rules[i].term, b->rules[j].term))
        return false;
  }
  return true;
}
static uint32_t get(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
         (uint32_t)p[3] << 24;
}
static void put(uint8_t *p, uint32_t v) {
  for (unsigned i = 0; i < 4; i++)
    p[i] = (uint8_t)(v >> (8 * i));
}
static uint32_t crc(const uint8_t *p) {
  uint32_t c = UINT32_MAX;
  for (unsigned i = 0; i < ROBOT_BANK_RECORD_SIZE - 4; i++) {
    if (i >= 12 && i < 16)
      continue;
    c ^= p[i];
    for (unsigned j = 0; j < 8; j++)
      c = (c >> 1) ^ ((c & 1) ? 0xedb88320u : 0);
  }
  return ~c;
}
static void voice_write(uint8_t *p, const RobotVoice *v) {
  p[0] = v->pitch;
  p[1] = v->speed;
  p[2] = v->mouth;
  p[3] = v->throat;
  p[4] = v->expression;
  p[5] = v->metallic;
  p[6] = v->roughness;
  p[7] = v->sing;
}
static RobotVoice voice_read(const uint8_t *p) {
  return (RobotVoice){p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7] != 0};
}
bool robot_bank_encode(const RobotBank *b, uint32_t sequence, uint8_t *out) {
  if (!robot_bank_valid(b))
    return false;
  memset(out, 0xff, ROBOT_BANK_RECORD_SIZE);
  memcpy(out, "RSAB", 4);
  put(out + 4, 2);
  put(out + 8, sequence);
  out[16] = (uint8_t)b->volume;
  out[17] = (uint8_t)(b->volume >> 8);
  out[18] = b->autoload;
  out[19] = b->startup;
  put(out + 20, b->baud);
  out[24] = b->framing;
  out[25] = b->rule_count;
  for (unsigned i = 0; i < 8; i++) {
    uint8_t *p = out + 32 + i * 33;
    memset(p, 0, 33);
    p[0] = b->characters[i].enabled;
    if (p[0]) {
      strcpy((char *)p + 1, b->characters[i].name);
      voice_write(p + 25, &b->characters[i].voice);
    }
  }
  for (unsigned i = 0; i < b->rule_count; i++) {
    uint8_t *p = out + 296 + i * 80;
    memset(p, 0, 80);
    strcpy((char *)p, b->rules[i].term);
    strcpy((char *)p + 16, b->rules[i].spoken);
  }
  put(out + 12, crc(out));
  put(out + ROBOT_BANK_RECORD_SIZE - 4, 0x5a17c0de);
  return true;
}
bool robot_bank_decode(const uint8_t *p, RobotBank *out, uint32_t *sequence) {
  if (memcmp(p, "RSAB", 4) || get(p + 4) != 2 || get(p + 12) != crc(p) ||
      get(p + ROBOT_BANK_RECORD_SIZE - 4) != 0x5a17c0de)
    return false;
  unsigned volume = p[16] | (unsigned)p[17] << 8;
  if (volume > 400 || p[18] > 1 || p[19] >= 8 || p[24] > 1 || p[25] > 64 ||
      !robot_bank_baud(get(p + 20)) || p[32 + p[19] * 33] != 1)
    return false;
  for (unsigned i = 0; i < 8; i++) {
    const uint8_t *c = p + 32 + i * 33;
    if (c[0] > 1)
      return false;
    if (c[0]) {
      RobotVoice v = voice_read(c + 25);
      if (!memchr(c + 1, 0, 24) || !robot_bank_name((const char *)c + 1) ||
          c[32] > 1 || !voice_valid(&v))
        return false;
    }
  }
  for (unsigned i = 0; i < p[25]; i++) {
    const char *r = (const char *)p + 296 + i * 80;
    if (!memchr(r, 0, 16) || !memchr(r + 16, 0, 64) || !rule_valid(r, r + 16))
      return false;
    for (unsigned j = 0; j < i; j++)
      if (same(r, (const char *)p + 296 + j * 80))
        return false;
  }
  if (out) {
    memset(out, 0, sizeof(*out));
    out->volume = (uint16_t)volume;
    out->autoload = p[18] != 0;
    out->startup = p[19];
    out->baud = get(p + 20);
    out->framing = p[24];
    out->rule_count = p[25];
    for (unsigned i = 0; i < 8; i++) {
      const uint8_t *c = p + 32 + i * 33;
      out->characters[i].enabled = c[0] != 0;
      if (c[0]) {
        strcpy(out->characters[i].name, (const char *)c + 1);
        out->characters[i].voice = voice_read(c + 25);
      }
    }
    memcpy(out->rules, p + 296, out->rule_count * sizeof(RobotRule));
  }
  if (sequence)
    *sequence = get(p + 8);
  return true;
}
int robot_bank_select(const uint8_t *a, const uint8_t *b, RobotBank *out,
                      uint32_t *seq) {
  uint32_t x = 0, y = 0;
  bool av = robot_bank_decode(a, NULL, &x), bv = robot_bank_decode(b, NULL, &y);
  if (!av && !bv)
    return -1;
  uint32_t d = y - x;
  int slot = bv && (!av || (d && d < 0x80000000u)) ? 1 : 0;
  robot_bank_decode(slot ? b : a, out, seq);
  return slot;
}
