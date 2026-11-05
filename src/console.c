#include "robot_console.h"
#include "robot_pcm.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usb(RobotConsole *c, const char *format, ...) {
  char out[384];
  va_list args;
  va_start(args, format);
  vsnprintf(out, sizeof(out) - 2, format, args);
  va_end(args);
  size_t n = strlen(out);
  out[n++] = '\r';
  out[n++] = '\n';
  c->ops.write(c->ops.context, ROBOT_USB, out, n);
}
static void byte(RobotConsole *c, RobotPort port, char b) {
  c->ops.write(c->ops.context, port, &b, 1);
}
static void error(RobotConsole *c, const char *message, bool compact,
                  RobotPort port) {
  snprintf(c->error, sizeof(c->error), "%s", message);
  if (compact)
    byte(c, port, 'E');
  else
    usb(c, "ERR %s", message);
}
static bool number(const char *s, unsigned max, unsigned *out) {
  if (!*s)
    return false;
  unsigned n = 0;
  for (; *s; s++) {
    if (*s < '0' || *s > '9' || (unsigned)(*s - '0') > max ||
        n > (max - (unsigned)(*s - '0')) / 10u)
      return false;
    n = n * 10u + (unsigned)(*s - '0');
  }
  if (n > max)
    return false;
  *out = n;
  return true;
}
static bool idle(RobotConsole *c) {
  if (c->busy) {
    error(c, "busy", false, ROBOT_USB);
    return false;
  }
  return true;
}
static bool exists(const RobotBank *b, unsigned id) {
  return id < 8 && b->characters[id].enabled;
}
static void reset_state(RobotConsole *c) {
  c->uart.id = c->active.startup;
  c->uart.voice = c->active.characters[c->uart.id].voice;
  c->uart.volume = c->active.volume;
  c->sim = c->uart;
  c->ops.volume(c->ops.context, c->uart.volume);
}
static void apply(RobotConsole *c) {
  c->active = c->edit;
  c->dirty = false;
  reset_state(c);
}
void robot_console_init(RobotConsole *c, const RobotConsoleOps *ops,
                        const RobotBank *saved, bool present, bool migrated) {
  memset(c, 0, sizeof(*c));
  c->ops = *ops;
  c->saved = present;
  c->migrated = migrated;
  if (present && saved->autoload)
    c->active = *saved;
  else {
    robot_bank_defaults(&c->active);
    if (present)
      c->active.autoload = saved->autoload;
    if (present) {
      c->active.baud = saved->baud;
      c->active.framing = saved->framing;
    }
  }
  c->edit = c->active;
  c->edit_id = c->edit.startup;
  reset_state(c);
  c->uart_baud = c->active.baud;
  c->uart_framing = c->active.framing;
  c->ops.uart(c->ops.context, c->uart_baud, c->uart_framing);
}
static void speak(RobotConsole *c, RobotSource source, const char *text,
                  bool phonetic) {
  bool compact = source != ROBOT_FROM_EDITOR;
  RobotPort port = source == ROBOT_FROM_UART ? ROBOT_UART : ROBOT_USB;
  if (c->busy) {
    error(c, "busy", compact, port);
    return;
  }
  size_t n = strlen(text);
  if (!n || n > 255 || (phonetic && n > 95)) {
    error(c, "speech length limit", compact, port);
    return;
  }
  RobotSpeechState *s = source == ROBOT_FROM_UART ? &c->uart : &c->sim;
  const RobotVoice *voice = source == ROBOT_FROM_EDITOR
                                ? &c->edit.characters[c->edit_id].voice
                                : &s->voice;
  const RobotBank *bank = source == ROBOT_FROM_EDITOR ? &c->edit : &c->active;
  if (source == ROBOT_FROM_EDITOR && !exists(bank, c->edit_id)) {
    error(c, "select an existing voice", false, port);
    return;
  }
  c->ops.volume(c->ops.context,
                source == ROBOT_FROM_EDITOR ? c->edit.volume : s->volume);
  if (!c->ops.speak(c->ops.context, voice, bank->rules, bank->rule_count, text,
                    phonetic)) {
    c->ops.volume(c->ops.context, c->uart.volume);
    error(c, "speech start failed", compact, port);
    return;
  }
  c->owner = source;
  c->busy = true;
  c->abandoned = false;
  if (compact)
    byte(c, port, 'A');
  else
    usb(c, "ACCEPTED");
}
static void compact(RobotConsole *c, RobotPort port, const char *line) {
  RobotSource source = port == ROBOT_UART ? ROBOT_FROM_UART : ROBOT_FROM_SIM;
  RobotSpeechState *s = port == ROBOT_UART ? &c->uart : &c->sim;
  if (!strcmp(line, "?")) {
    byte(c, port, c->busy ? 'B' : 'R');
    return;
  }
  if (!strcmp(line, "!")) {
    if (!c->busy) {
      byte(c, port, 'R');
      return;
    }
    if (c->owner != source) {
      error(c, "speech owned by other connection", true, port);
      return;
    }
    c->ops.stop(c->ops.context);
    byte(c, port, 'A');
    return;
  }
  if (line[0] && line[1] == ' ' && (line[0] == 'S' || line[0] == 'K')) {
    speak(c, source, line + 2, line[0] == 'K');
    return;
  }
  if (c->busy && c->owner != source) {
    error(c, "speech owned by other connection", true, port);
    return;
  }
  unsigned v;
  if (!line[0] || line[1] != ' ' || !number(line + 2, 400, &v)) {
    error(c, "invalid speech command", true, port);
    return;
  }
  if (line[0] == 'C') {
    if (!exists(&c->active, v)) {
      error(c, "unknown voice ID", true, port);
      return;
    }
    s->id = v;
    s->voice = c->active.characters[v].voice;
  } else if (line[0] == 'V') {
    s->volume = v;
    if (c->busy && c->owner == source)
      c->ops.volume(c->ops.context, v);
  } else {
    const char *key = NULL;
    switch (line[0]) {
    case 'P':
      key = "PITCH";
      break;
    case 'T':
      key = "SPEED";
      break;
    case 'M':
      key = "MOUTH";
      break;
    case 'H':
      key = "THROAT";
      break;
    case 'N':
      key = "SING";
      break;
    case 'X':
      key = "EXPRESSION";
      break;
    case 'Y':
      key = "METALLIC";
      break;
    case 'Z':
      key = "ROUGHNESS";
      break;
    }
    if (!key || !robot_bank_voice_set(&s->voice, key, v)) {
      error(c, "invalid voice tweak", true, port);
      return;
    }
  }
  byte(c, port, 'A');
}
static void show(RobotConsole *c, unsigned id) {
  const RobotCharacter *v = &c->edit.characters[id];
  usb(c,
      "VOICE %u %s PITCH=%u SPEED=%u MOUTH=%u THROAT=%u SING=%u EXPRESSION=%u "
      "METALLIC=%u ROUGHNESS=%u",
      id, v->name, v->voice.pitch, v->voice.speed, v->voice.mouth,
      v->voice.throat, v->voice.sing, v->voice.expression, v->voice.metallic,
      v->voice.roughness);
}
static void help(RobotConsole *c, const char *topic) {
  if (!*topic) {
    usb(c, "Robot SAM USB editor. Send one complete ASCII line; no interactive "
           "key editing.");
    usb(c, "HELP VOICE | HELP DICTIONARY | HELP UART | HELP TEST | HELP SAVE");
    usb(c, "STATUS | ERROR | SAY text | PHONEMES symbols | STOP | APPLY | "
           "DISCARD");
    return;
  }
  if (!strcmp(topic, "VOICE")) {
    usb(c, "VOICE LIST | VOICE SHOW id | VOICE SELECT id (USB audition/edit "
           "target)");
    usb(c, "VOICE CREATE id NAME | VOICE RENAME id NAME | VOICE DELETE id");
    usb(c, "VOICE PRESET id "
           "CLASSIC|COMPANION|GUARDIAN|ENFORCER|DICTATOR|MALFUNCTION");
    usb(c, "VOICE SET id PITCH|SPEED|MOUTH|THROAT 0..255; SING 0|1; "
           "EXPRESSION|METALLIC|ROUGHNESS 0..100");
    usb(c, "SET key value edits selected voice. SET STARTUP id | SET VOLUME "
           "0..400");
    usb(c, "Example: VOICE CREATE 6 ENEMY, then VOICE SET 6 PITCH 93 (separate "
           "lines).");
  } else if (!strcmp(topic, "DICTIONARY")) {
    usb(c, "DICT LIST | DICT SHOW term | DICT SET term=spoken words | DICT "
           "DELETE term | DICT CLEAR");
    usb(c, "64 whole-token overrides; term 1..15 letters/digits/hyphens; "
           "speech 1..63 letters/spaces/apostrophes.");
    usb(c, "Example: DICT SET operational=opperational. PRONOUNCE is an alias "
           "for DICT SET.");
    usb(c, "Edits are temporary until APPLY and SAVE_CONFIG. Rules apply to "
           "all characters.");
  } else if (!strcmp(topic, "UART")) {
    usb(c, "UART SHOW | UART BAUD 1200|2400|4800|9600|19200|38400|57600|115200 "
           "| UART FRAME 8N1|7E1");
    usb(c, "APPLY activates voice/dictionary configuration; UART APPLY "
           "explicitly changes live serial settings while idle.");
    usb(c, "SAVE_CONFIG persists applied baud/framing for next boot. Default "
           "9600 8N1; migrated 0.4.0 keeps 115200.");
    usb(c, "UART: S text | K phonemes | C id | V volume | P pitch | T speed | "
           "M mouth | H throat");
    usb(c, "N sing | X expression | Y metallic | Z roughness | ! stop | ? "
           "state. Every command ends CR/LF.");
    usb(c, "Replies are bare bytes: A accepted, D speech done/ready, R "
           "ready/stopped, B busy query, E error.");
  } else if (!strcmp(topic, "TEST")) {
    usb(c, "SAY text and PHONEMES symbols audition the selected workspace "
           "voice/dictionary on USB only.");
    usb(c, "!~ on its own line enters UART simulation (reply R). ~! on its own "
           "line exits while idle.");
    usb(c, "Simulation uses the identical byte-fed UART parser and bare "
           "A/D/R/B/E responses, with isolated temporary tweaks.");
    usb(c, "Both modes use line delimiters. USB simulation cannot test actual "
           "baud/parity/wiring.");
  } else if (!strcmp(topic, "SAVE")) {
    usb(c, "APPLY commits workspace to runtime while idle. DISCARD restores "
           "workspace from applied config.");
    usb(c, "SAVE_CONFIG saves applied config, including all 8 slots, 64 rules, "
           "volume and UART settings; APPLY first.");
    usb(c, "LOAD_CONFIG restores saved workspace/runtime (UART baud changes "
           "only on UART APPLY or reboot).");
    usb(c, "DEFAULTS resets workspace only. ERASE_CONFIG removes saved data, "
           "leaving RAM unchanged.");
    usb(c, "SET AUTOLOAD 0|1, APPLY, SAVE_CONFIG controls boot restore. All "
           "flash/apply/load operations require idle.");
  } else
    error(c, "unknown help topic; use HELP", false, ROBOT_USB);
}
static void editor(RobotConsole *c, char *line) {
  if (!strcmp(line, "~!")) {
    if (idle(c))
      usb(c, "OK editor");
    return;
  }
  if (!strcmp(line, "!~")) {
    if (idle(c)) {
      c->usb_sim = true;
      c->sim = c->uart;
      byte(c, ROBOT_USB, 'R');
    }
    return;
  }
  if (!strcmp(line, "HELP")) {
    help(c, "");
    return;
  }
  if (!strncmp(line, "HELP ", 5)) {
    help(c, line + 5);
    return;
  }
  if (!strcmp(line, "ERROR")) {
    usb(c, "LAST_ERROR %s", *c->error ? c->error : "none");
    return;
  }
  if (!strcmp(line, "STATUS")) {
    usb(c,
        "%s version=0.5.0 audio=%s audio_owner=%s saved=%u migrated=%u "
        "dirty=%u edit_voice=%u "
        "startup=%u volume=%u edit_volume=%u rules=%u UART=%u/%s autoload=%u "
        "underruns=%u clipped=%u",
        c->busy ? "BUSY" : "IDLE", c->ops.backend ? c->ops.backend : "unknown",
        !c->busy                      ? "none"
        : c->owner == ROBOT_FROM_UART ? "UART"
        : c->owner == ROBOT_FROM_SIM  ? "USB_SIM"
                                      : "USB",
        c->saved, c->migrated, c->dirty, c->edit_id, c->edit.startup,
        c->uart.volume, c->edit.volume, c->edit.rule_count, c->uart_baud,
        c->uart_framing ? "7E1" : "8N1", c->edit.autoload, c->underruns,
        c->clipped);
    return;
  }
  if (!strncmp(line, "SAY ", 4)) {
    speak(c, ROBOT_FROM_EDITOR, line + 4, false);
    return;
  }
  if (!strncmp(line, "PHONEMES ", 9)) {
    speak(c, ROBOT_FROM_EDITOR, line + 9, true);
    return;
  }
  if (!strcmp(line, "STOP")) {
    if (c->busy) {
      c->ops.stop(c->ops.context);
      usb(c, "OK stopping");
    } else
      usb(c, "OK idle");
    return;
  }
  if (!strcmp(line, "APPLY")) {
    if (idle(c)) {
      if (!robot_bank_valid(&c->edit)) {
        error(c, "invalid workspace", false, ROBOT_USB);
        return;
      }
      apply(c);
      usb(c, "OK applied");
    }
    return;
  }
  if (!strcmp(line, "DISCARD")) {
    c->edit = c->active;
    c->edit_id = c->edit.startup;
    c->dirty = false;
    usb(c, "OK discarded");
    return;
  }
  if (!strcmp(line, "DEFAULTS")) {
    robot_bank_defaults(&c->edit);
    c->edit_id = c->edit.startup;
    c->dirty = true;
    usb(c, "OK defaults in workspace; APPLY to activate");
    return;
  }
  if (!strcmp(line, "SAVE_CONFIG")) {
    if (!idle(c))
      return;
    if (c->dirty) {
      error(c, "APPLY edits before saving", false, ROBOT_USB);
      return;
    }
    int result = c->ops.save(c->ops.context, &c->active);
    if (result < 0)
      error(c, "flash save failed", false, ROBOT_USB);
    else {
      c->saved = true;
      c->migrated = false;
      usb(c, result ? "OK saved" : "OK unchanged");
    }
    return;
  }
  if (!strcmp(line, "LOAD_CONFIG")) {
    if (!idle(c))
      return;
    bool migrated = false;
    if (!c->ops.load(c->ops.context, &c->edit, &migrated)) {
      c->saved = false;
      c->migrated = false;
      error(c, "no saved configuration", false, ROBOT_USB);
      return;
    }
    apply(c);
    c->edit_id = c->edit.startup;
    c->saved = true;
    c->migrated = migrated;
    usb(c, "OK loaded; use UART APPLY to change live serial settings");
    return;
  }
  if (!strcmp(line, "ERASE_CONFIG")) {
    if (!idle(c))
      return;
    if (!c->ops.erase(c->ops.context))
      error(c, "flash erase failed", false, ROBOT_USB);
    else {
      c->saved = false;
      c->migrated = false;
      usb(c, "OK erased");
    }
    return;
  }
  if (!strcmp(line, "UART SHOW")) {
    usb(c, "UART live=%u/%s workspace=%u/%s", c->uart_baud,
        c->uart_framing ? "7E1" : "8N1", c->edit.baud,
        c->edit.framing ? "7E1" : "8N1");
    return;
  }
  if (!strcmp(line, "UART APPLY")) {
    if (!idle(c))
      return;
    if (c->dirty) {
      error(c, "APPLY workspace first", false, ROBOT_USB);
      return;
    }
    if (!c->ops.uart(c->ops.context, c->active.baud, c->active.framing)) {
      error(c, "UART apply failed", false, ROBOT_USB);
      return;
    }
    c->uart_baud = c->active.baud;
    c->uart_framing = c->active.framing;
    memset(&c->lines[ROBOT_UART], 0, sizeof(RobotInputLine));
    usb(c, "OK UART applied");
    return;
  }
  unsigned value;
  if (!strncmp(line, "UART BAUD ", 10)) {
    if (!number(line + 10, 115200, &value) || !robot_bank_baud(value)) {
      error(c, "unsupported baud", false, ROBOT_USB);
      return;
    }
    c->edit.baud = value;
    c->dirty = true;
    usb(c, "OK workspace");
    return;
  }
  if (!strncmp(line, "UART FRAME ", 11)) {
    if (strcmp(line + 11, "8N1") && strcmp(line + 11, "7E1")) {
      error(c, "use 8N1 or 7E1", false, ROBOT_USB);
      return;
    }
    c->edit.framing = !strcmp(line + 11, "7E1");
    c->dirty = true;
    usb(c, "OK workspace");
    return;
  }
  if (!strcmp(line, "VOICE LIST")) {
    for (unsigned i = 0; i < 8; i++)
      if (exists(&c->edit, i))
        usb(c, "VOICE %u %s", i, c->edit.characters[i].name);
    usb(c, "OK end");
    return;
  }
  if (!strncmp(line, "CHARACTER ", 10)) {
    for (unsigned i = 0; i < 8; i++)
      if (exists(&c->edit, i)) {
        char name[24];
        snprintf(name, sizeof(name), "%s", line + 10);
        for (char *p = name; *p; p++)
          if (*p >= 'a' && *p <= 'z')
            *p -= 32;
        char stored[24];
        strcpy(stored, c->edit.characters[i].name);
        for (char *p = stored; *p; p++)
          if (*p >= 'a' && *p <= 'z')
            *p -= 32;
        if (strlen(line + 10) < 24 && !strcmp(name, stored)) {
          c->edit_id = i;
          usb(c, "OK selected %u", i);
          return;
        }
      }
    error(c, "unknown character; VOICE LIST", false, ROBOT_USB);
    return;
  }
  char action[16], arg[32], key[24], val[24], id_text[16], extra;
  unsigned id;
  if (sscanf(line, "VOICE %15s %15s %23s %23s %c", action, id_text, key, val,
             &extra) >= 2 &&
      !strncmp(line, "VOICE ", 6)) {
    if (!number(id_text, 7, &id)) {
      error(c, "voice ID must be 0..7", false, ROBOT_USB);
      return;
    }
    int n = sscanf(line, "VOICE %15s %15s %23s %23s %c", action, id_text, key,
                   val, &extra);
    RobotCharacter *v = &c->edit.characters[id];
    if (n == 2 && !strcmp(action, "SHOW") && v->enabled) {
      show(c, id);
      return;
    }
    if (n == 2 && !strcmp(action, "SELECT") && v->enabled) {
      c->edit_id = id;
      usb(c, "OK selected %u", id);
      return;
    }
    if (n == 2 && !strcmp(action, "DELETE") && v->enabled &&
        id != c->edit.startup) {
      memset(v, 0, sizeof(*v));
      if (c->edit_id == id)
        c->edit_id = c->edit.startup;
      c->dirty = true;
      usb(c, "OK workspace");
      return;
    }
    if (n == 3 && (!strcmp(action, "CREATE") || !strcmp(action, "RENAME")) &&
        robot_bank_name(key)) {
      if ((!strcmp(action, "CREATE") && v->enabled) ||
          (!strcmp(action, "RENAME") && !v->enabled)) {
        error(c, "slot already exists or is missing", false, ROBOT_USB);
        return;
      }
      if (!v->enabled) {
        Robot r;
        robot_init(&r);
        robot_character(&r, "CLASSIC");
        v->voice = r.voice;
        v->enabled = true;
      }
      strcpy(v->name, key);
      c->dirty = true;
      usb(c, "OK workspace");
      return;
    }
    if (n == 3 && !strcmp(action, "PRESET")) {
      Robot r;
      robot_init(&r);
      if (!robot_character(&r, key)) {
        error(c, "unknown preset", false, ROBOT_USB);
        return;
      }
      v->voice = r.voice;
      if (!v->enabled)
        strcpy(v->name, key);
      v->enabled = true;
      c->dirty = true;
      usb(c, "OK workspace");
      return;
    }
    if (n == 4 && !strcmp(action, "SET") && v->enabled &&
        number(val, 400, &value) &&
        robot_bank_voice_set(&v->voice, key, value)) {
      c->dirty = true;
      usb(c, "OK workspace");
      return;
    }
    error(c, "invalid voice command; HELP VOICE", false, ROBOT_USB);
    return;
  }
  if (!strncmp(line, "SET ", 4) &&
      sscanf(line + 4, "%31s %23s %c", arg, val, &extra) == 2 &&
      number(val, 400, &value)) {
    for (char *p = arg; *p; p++)
      if (*p >= 'a' && *p <= 'z')
        *p -= 32;
    bool ok = true;
    if (!strcmp(arg, "VOLUME")) {
      c->edit.volume = (uint16_t)value;
      if (c->busy && c->owner == ROBOT_FROM_EDITOR)
        c->ops.volume(c->ops.context, value);
    } else if (!strcmp(arg, "AUTOLOAD") && value <= 1)
      c->edit.autoload = value != 0;
    else if (!strcmp(arg, "STARTUP") && exists(&c->edit, value))
      c->edit.startup = (uint8_t)value;
    else
      ok = exists(&c->edit, c->edit_id) &&
           robot_bank_voice_set(&c->edit.characters[c->edit_id].voice, arg,
                                value);
    if (ok) {
      c->dirty = true;
      usb(c, "OK workspace");
    } else
      error(c, "invalid setting", false, ROBOT_USB);
    return;
  }
  if (!strcmp(line, "DICT LIST")) {
    for (unsigned i = 0; i < c->edit.rule_count; i++)
      usb(c, "DICT %s=%s", c->edit.rules[i].term, c->edit.rules[i].spoken);
    usb(c, "OK end");
    return;
  }
  if (!strcmp(line, "DICT CLEAR")) {
    memset(c->edit.rules, 0, sizeof(c->edit.rules));
    c->edit.rule_count = 0;
    c->dirty = true;
    usb(c, "OK workspace");
    return;
  }
  if (!strncmp(line, "DICT SET ", 9) || !strncmp(line, "PRONOUNCE ", 10)) {
    char *term = line + (line[0] == 'D' ? 9 : 10),
         *separator = strchr(term, '=');
    bool ok = separator != NULL;
    if (ok) {
      *separator = 0;
      ok = robot_bank_rule(&c->edit, term, separator + 1);
    }
    if (ok) {
      c->dirty = true;
      usb(c, "OK workspace");
    } else
      error(c, "invalid dictionary entry or full", false, ROBOT_USB);
    return;
  }
  if (!strncmp(line, "DICT DELETE ", 12)) {
    if (robot_bank_delete_rule(&c->edit, line + 12)) {
      c->dirty = true;
      usb(c, "OK workspace");
    } else
      error(c, "unknown dictionary term", false, ROBOT_USB);
    return;
  }
  if (!strncmp(line, "DICT SHOW ", 10) && strlen(line + 10) < 16) {
    for (unsigned i = 0; i < c->edit.rule_count; i++) {
      char a[16], b[16];
      snprintf(a, sizeof(a), "%s", line + 10);
      strcpy(b, c->edit.rules[i].term);
      for (char *p = a; *p; p++)
        if (*p >= 'a' && *p <= 'z')
          *p -= 32;
      for (char *p = b; *p; p++)
        if (*p >= 'a' && *p <= 'z')
          *p -= 32;
      if (!strcmp(a, b)) {
        usb(c, "DICT %s=%s", c->edit.rules[i].term, c->edit.rules[i].spoken);
        return;
      }
    }
    error(c, "unknown dictionary term", false, ROBOT_USB);
    return;
  }
  error(c, "unknown command; HELP lists topics", false, ROBOT_USB);
}
void robot_console_feed(RobotConsole *c, RobotPort port, unsigned b) {
  RobotInputLine *line = &c->lines[port];
  if (b == '\r' || b == '\n') {
    bool is_compact = port == ROBOT_UART || c->usb_sim;
    if (line->invalid)
      error(c, "invalid ASCII or command too long", is_compact, port);
    else if (line->used) {
      line->bytes[line->used] = 0;
      if (port == ROBOT_USB && c->usb_sim &&
          (!strcmp(line->bytes, "~!") || !strcmp(line->bytes, "!~"))) {
        if (c->busy)
          error(c, "busy", true, port);
        else if (line->bytes[0] == '~') {
          c->usb_sim = false;
          usb(c, "OK editor");
        } else
          byte(c, port, 'R');
      } else if (is_compact)
        compact(c, port, line->bytes);
      else
        editor(c, line->bytes);
    }
    line->used = 0;
    line->invalid = false;
    return;
  }
  if (b < 32 || b > 126) {
    line->invalid = true;
    return;
  }
  if (line->used + 1 < sizeof(line->bytes))
    line->bytes[line->used++] = (char)b;
  else
    line->invalid = true;
}
void robot_console_uart_error(RobotConsole *c) {
  c->lines[ROBOT_UART].invalid = true;
}
void robot_console_event(RobotConsole *c, RobotEvent event, const char *message,
                         unsigned underruns, unsigned clipped) {
  if (!c->busy)
    return;
  RobotPort port = c->owner == ROBOT_FROM_UART ? ROBOT_UART : ROBOT_USB;
  bool compact_mode = c->owner != ROBOT_FROM_EDITOR;
  if (event == ROBOT_STARTED) {
    if (!compact_mode && !c->abandoned)
      usb(c, "STARTED");
    return;
  }
  c->busy = false;
  c->underruns = underruns;
  c->clipped = clipped;
  if (event == ROBOT_FAILED) {
    snprintf(c->error, sizeof(c->error), "%s",
             message ? message : "synthesis failed");
    if (!c->abandoned) {
      if (compact_mode)
        byte(c, port, 'E');
      else
        usb(c, "ERR %s", c->error);
    }
  } else if (!c->abandoned) {
    if (compact_mode)
      byte(c, port, event == ROBOT_DONE ? 'D' : 'R');
    else
      usb(c, "%s underruns=%u clipped=%u",
          event == ROBOT_DONE ? "DONE" : "STOPPED", underruns, clipped);
  }
  c->ops.volume(c->ops.context, c->uart.volume);
  c->abandoned = false;
}
void robot_console_usb_reset(RobotConsole *c) {
  memset(&c->lines[ROBOT_USB], 0, sizeof(RobotInputLine));
  c->usb_sim = false;
  if (c->busy && c->owner != ROBOT_FROM_UART) {
    c->abandoned = true;
    c->ops.stop(c->ops.context);
  }
}
