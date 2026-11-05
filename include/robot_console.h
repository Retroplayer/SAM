#ifndef ROBOT_CONSOLE_H
#define ROBOT_CONSOLE_H
#include "robot_bank.h"
typedef enum { ROBOT_UART, ROBOT_USB } RobotPort;
typedef enum { ROBOT_FROM_UART, ROBOT_FROM_EDITOR, ROBOT_FROM_SIM } RobotSource;
typedef enum {
  ROBOT_STARTED,
  ROBOT_DONE,
  ROBOT_STOPPED,
  ROBOT_FAILED
} RobotEvent;
typedef struct {
  void *context;
  void (*write)(void *, RobotPort, const char *, size_t);
  bool (*speak)(void *, const RobotVoice *, const RobotRule *, unsigned,
                const char *, bool);
  void (*stop)(void *);
  void (*volume)(void *, unsigned);
  bool (*uart)(void *, unsigned, unsigned);
  int (*save)(void *, const RobotBank *);
  bool (*load)(void *, RobotBank *, bool *);
  bool (*erase)(void *);
  const char *backend;
} RobotConsoleOps;
typedef struct {
  RobotVoice voice;
  unsigned volume, id;
} RobotSpeechState;
typedef struct {
  char bytes[272];
  size_t used;
  bool invalid;
} RobotInputLine;
typedef struct {
  RobotBank active, edit;
  RobotSpeechState uart, sim;
  RobotConsoleOps ops;
  RobotInputLine lines[2];
  RobotSource owner;
  bool busy, usb_sim, dirty, saved, migrated, abandoned;
  unsigned edit_id, uart_baud, uart_framing;
  unsigned underruns, clipped;
  char error[96];
} RobotConsole;
void robot_console_init(RobotConsole *, const RobotConsoleOps *,
                        const RobotBank *, bool saved, bool migrated);
void robot_console_feed(RobotConsole *, RobotPort, unsigned byte);
void robot_console_uart_error(RobotConsole *);
void robot_console_event(RobotConsole *, RobotEvent, const char *error,
                         unsigned underruns, unsigned clipped);
void robot_console_usb_reset(RobotConsole *);
#endif
