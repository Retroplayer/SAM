#include "robot_console.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static RobotConsole console;
static RobotBank saved_bank;
static char output[2][65536];
static unsigned counts[2], serial_calls, serial_baud, serial_frame, volume_now,
    save_calls, stops;
static bool has_saved;
static RobotVoice job_voice;
static RobotRule job_rules[64];
static unsigned job_count;
static char job_text[256];
static void write_out(void *ctx, RobotPort port, const char *s, size_t n) {
  (void)ctx;
  assert(counts[port] + n < sizeof(output[port]));
  for (size_t i = 0; i < n; i++)
    assert((unsigned char)s[i] < 128);
  memcpy(output[port] + counts[port], s, n);
  counts[port] += (unsigned)n;
  output[port][counts[port]] = 0;
}
static bool speak(void *ctx, const RobotVoice *v, const RobotRule *r,
                  unsigned n, const char *text, bool phonetic) {
  (void)ctx;
  (void)phonetic;
  job_voice = *v;
  job_count = n;
  memcpy(job_rules, r, n * sizeof(RobotRule));
  strcpy(job_text, text);
  return true;
}
static void stop(void *ctx) {
  (void)ctx;
  stops++;
}
static void volume(void *ctx, unsigned n) {
  (void)ctx;
  volume_now = n;
}
static bool uart(void *ctx, unsigned baud, unsigned frame) {
  (void)ctx;
  serial_calls++;
  serial_baud = baud;
  serial_frame = frame;
  return true;
}
static int save(void *ctx, const RobotBank *bank) {
  (void)ctx;
  saved_bank = *bank;
  has_saved = true;
  save_calls++;
  return 1;
}
static bool load(void *ctx, RobotBank *bank, bool *migrated) {
  (void)ctx;
  if (!has_saved)
    return false;
  *bank = saved_bank;
  *migrated = false;
  return true;
}
static bool erase(void *ctx) {
  (void)ctx;
  has_saved = false;
  return true;
}
static void clear(void) {
  memset(output, 0, sizeof(output));
  counts[0] = counts[1] = 0;
}
static void feed(RobotPort p, const char *s) {
  while (*s)
    robot_console_feed(&console, p, (unsigned char)*s++);
}
static void command(RobotPort p, const char *s) {
  feed(p, s);
  feed(p, "\n");
}
static void done(void) {
  robot_console_event(&console, ROBOT_DONE, NULL, 0, 0);
}
int main(void) {
  RobotConsoleOps ops = {NULL, write_out, speak, stop,  volume,
                         uart, save,      load,  erase, "TEST"};
  robot_console_init(&console, &ops, NULL, false, false);
  assert(serial_baud == 9600 && serial_frame == 0 && volume_now == 118);
  assert(!counts[0] && !counts[1]);
  command(ROBOT_USB, "LOAD_CONFIG");
  assert(strstr(output[1], "ERR no saved configuration"));
  clear();
  feed(ROBOT_USB, "SET PITCH 93");
  assert(!console.dirty && console.uart.voice.pitch == 58);
  feed(ROBOT_USB, "\r\n");
  assert(console.dirty && console.edit.characters[1].voice.pitch == 93);
  assert(console.uart.voice.pitch == 58 && !counts[0]);
  command(ROBOT_USB, "SET VOLUME 200");
  assert(volume_now == 118);
  command(ROBOT_USB, "DICT SET operational=opperational");
  clear();
  command(ROBOT_USB, "SAY operational");
  assert(console.busy && volume_now == 200 && job_count == 1);
  assert(job_voice.pitch == 93 && !strcmp(job_rules[0].spoken, "opperational"));
  assert(!strcmp(output[1], "ACCEPTED\r\n") && !counts[0]);
  command(ROBOT_UART, "S no");
  command(ROBOT_UART, "?");
  command(ROBOT_UART, "V 300");
  assert(!strcmp(output[0], "EBE") && volume_now == 200);
  command(ROBOT_USB, "SET VOLUME 220");
  assert(volume_now == 220);
  robot_console_event(&console, ROBOT_STARTED, NULL, 0, 0);
  done();
  assert(volume_now == 118);
  assert(strstr(output[1], "STARTED\r\nDONE underruns=0 clipped=0\r\n"));
  clear();
  command(ROBOT_USB, "SAVE_CONFIG");
  assert(!save_calls && strstr(output[1], "APPLY"));
  command(ROBOT_USB, "APPLY");
  assert(console.uart.voice.pitch == 93 && volume_now == 220);
  unsigned before = serial_calls;
  command(ROBOT_USB, "UART BAUD 1200");
  command(ROBOT_USB, "UART FRAME 7E1");
  command(ROBOT_USB, "APPLY");
  assert(serial_calls == before && console.uart_baud == 9600);
  command(ROBOT_USB, "SAVE_CONFIG");
  assert(save_calls == 1 && saved_bank.baud == 1200 && saved_bank.framing == 1);
  command(ROBOT_USB, "UART APPLY");
  assert(serial_calls == before + 1 && serial_baud == 1200 &&
         serial_frame == 1);
  clear();
  command(ROBOT_USB, "!~");
  assert(console.usb_sim && !strcmp(output[1], "R"));
  command(ROBOT_USB, "C 0");
  command(ROBOT_USB, "V 400");
  assert(!strcmp(output[1], "RAA"));
  assert(console.uart.volume == 220 && console.sim.volume == 400);
  clear();
  feed(ROBOT_USB, "S Hel");
  assert(!counts[1] && !console.busy);
  feed(ROBOT_USB, "lo\r\n");
  assert(!strcmp(output[1], "A") && !strcmp(job_text, "Hello") &&
         volume_now == 400);
  command(ROBOT_USB, "~!");
  assert(console.usb_sim && !strcmp(output[1], "AE"));
  clear();
  command(ROBOT_UART, "?");
  command(ROBOT_UART, "S rejected");
  assert(!strcmp(output[0], "BE"));
  robot_console_event(&console, ROBOT_STARTED, NULL, 0, 0);
  done();
  assert(!strcmp(output[1], "D"));
  clear();
  command(ROBOT_USB, "~!");
  assert(!console.usb_sim && !strcmp(output[1], "OK editor\r\n") && !counts[0]);
  clear();
  command(ROBOT_UART, "?");
  feed(ROBOT_UART, "S par");
  assert(!strcmp(output[0], "R"));
  feed(ROBOT_UART, "tial\n");
  assert(!strcmp(output[0], "RA") && !strcmp(job_text, "partial"));
  robot_console_event(&console, ROBOT_STARTED, NULL, 0, 0);
  assert(!counts[1]);
  command(ROBOT_USB, "HELP");
  assert(strstr(output[1], "HELP VOICE") && !strcmp(output[0], "RA"));
  done();
  assert(!strcmp(output[0], "RAD"));
  clear();
  command(ROBOT_UART, "S Stop test");
  command(ROBOT_UART, "!");
  assert(stops == 1 && !strcmp(output[0], "AA"));
  robot_console_event(&console, ROBOT_STOPPED, NULL, 0, 0);
  assert(!strcmp(output[0], "AAR"));
  command(ROBOT_UART, "!");
  assert(!strcmp(output[0], "AARR"));
  clear();
  command(ROBOT_UART, "SAVE_CONFIG");
  command(ROBOT_UART, "!~");
  assert(!strcmp(output[0], "EE") && !counts[1]);
  robot_console_feed(&console, ROBOT_UART, 0xff);
  feed(ROBOT_UART, "\n?\n");
  assert(!strcmp(output[0], "EEER"));
  clear();
  for (unsigned i = 0; i < 800; i++)
    robot_console_feed(&console, ROBOT_UART, 'A');
  feed(ROBOT_UART, "\n?\n");
  assert(!strcmp(output[0], "ER"));
  clear();
  command(ROBOT_USB, "VOICE CREATE 6 ENEMY");
  command(ROBOT_USB, "VOICE SET 6 PITCH 93");
  command(ROBOT_USB, "VOICE SET 6 SPEED 148");
  command(ROBOT_USB, "VOICE RENAME 6 CYLON");
  command(ROBOT_USB, "VOICE SELECT 6");
  command(ROBOT_USB, "SET MOUTH 101");
  command(ROBOT_USB, "SET THROAT 105");
  command(ROBOT_USB, "SET STARTUP 6");
  assert(console.edit.startup == 6 &&
         console.edit.characters[6].voice.speed == 148);
  command(ROBOT_USB, "VOICE DELETE 6");
  assert(console.edit.characters[6].enabled);
  command(ROBOT_USB, "DICT DELETE operational");
  assert(console.edit.rule_count == 0);
  for (unsigned i = 0; i < 64; i++) {
    char line[80];
    snprintf(line, sizeof(line), "DICT SET TERM%u=test word", i);
    command(ROBOT_USB, line);
  }
  assert(console.edit.rule_count == 64);
  command(ROBOT_USB, "DICT SET EXTRA=fail");
  assert(console.edit.rule_count == 64);
  command(ROBOT_USB, "DICT SET TERM63=changed word");
  assert(!strcmp(console.edit.rules[63].spoken, "changed word"));
  command(ROBOT_USB, "APPLY");
  command(ROBOT_USB, "SAVE_CONFIG");
  assert(console.uart.id == 6 && console.active.rule_count == 64);
  clear();
  command(ROBOT_UART, "S active dictionary");
  assert(job_count == 64 && job_voice.pitch == 93);
  command(ROBOT_USB, "ERASE_CONFIG");
  assert(strstr(output[1], "ERR busy"));
  command(ROBOT_USB, "VOICE SET 6 PITCH 70");
  assert(job_voice.pitch == 93);
  done();
  command(ROBOT_USB, "LOAD_CONFIG");
  assert(console.edit.characters[6].voice.pitch == 93 && !console.dirty);
  command(ROBOT_USB, "DEFAULTS");
  assert(console.dirty && console.active.startup == 6);
  command(ROBOT_USB, "DISCARD");
  assert(console.edit.startup == 6 && !console.dirty);
  command(ROBOT_USB, "!~");
  command(ROBOT_USB, "S literal ~! inside text");
  assert(console.usb_sim);
  done();
  command(ROBOT_USB, "S disconnect");
  unsigned old_stops = stops;
  robot_console_usb_reset(&console);
  assert(stops == old_stops + 1 && !console.usb_sim);
  clear();
  robot_console_event(&console, ROBOT_STOPPED, NULL, 0, 0);
  assert(!counts[0] && !counts[1]);
  command(ROBOT_UART, "S robot remains");
  old_stops = stops;
  robot_console_usb_reset(&console);
  assert(stops == old_stops);
  done();
  const char *topics[] = {"HELP VOICE", "HELP DICTIONARY", "HELP UART",
                          "HELP TEST", "HELP SAVE"};
  clear();
  for (unsigned i = 0; i < 5; i++)
    command(ROBOT_USB, topics[i]);
  assert(!counts[0] && !strstr(output[1], "ERR"));
  clear();
  command(ROBOT_UART, "S fail later");
  robot_console_event(&console, ROBOT_FAILED, "bad phonemes", 0, 0);
  assert(!strcmp(output[0], "AE") && !counts[1]);
  command(ROBOT_USB, "ERROR");
  assert(strstr(output[1], "bad phonemes"));
  clear();
  command(ROBOT_UART, "S operator stop");
  command(ROBOT_USB, "STOP");
  robot_console_event(&console, ROBOT_STOPPED, NULL, 0, 0);
  assert(!strcmp(output[0], "AR") && !strcmp(output[1], "OK stopping\r\n"));
  const char *tweaks[] = {"P 93", "T 148", "M 101", "H 105", "N 0",
                          "X 0",  "Y 0",   "Z 0",   "V 118"};
  clear();
  for (unsigned i = 0; i < 9; i++)
    command(ROBOT_UART, tweaks[i]);
  assert(!strcmp(output[0], "AAAAAAAAA"));
  assert(console.uart.voice.pitch == 93 && console.uart.voice.speed == 148 &&
         console.uart.volume == 118);
  command(ROBOT_UART, "P 256");
  command(ROBOT_UART, "N 2");
  command(ROBOT_UART, "V 401");
  assert(!strcmp(output[0], "AAAAAAAAAEEE"));
  saved_bank.autoload = false;
  clear();
  robot_console_init(&console, &ops, &saved_bank, true, false);
  assert(!console.active.autoload && console.active.rule_count == 0 &&
         console.uart.volume == 118);
  assert(serial_baud == saved_bank.baud && serial_frame == saved_bank.framing &&
         !counts[0]);
  command(ROBOT_USB, "LOAD_CONFIG");
  assert(console.active.rule_count == 64 && console.uart.id == 6);
  puts("Console checks passed: line buffering, shared compact parser, isolated "
       "routing/ownership, editing, 64 rules, simulation, stop, byte recovery, "
       "UART settings, saving, help.");
}
