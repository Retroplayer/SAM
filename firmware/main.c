#include "audio.h"
#include "bank_storage.h"
#include "hardware/uart.h"
#include "pico/flash.h"
#include "pico/multicore.h"
#include "pico/sem.h"
#include "pico/stdio_usb.h"
#include "pico/stdlib.h"
#include "pico/util/queue.h"
#include "robot_console.h"
#include "tusb.h"
#include <string.h>

static RobotConsole console;
static RobotBank boot;
static RobotPlan plan;
static Robot job_robot;
static RobotRule job_rules[ROBOT_DICTIONARY];
static unsigned job_count;
static char job_text[ROBOT_TEXT_MAX + 1];
static bool job_phonetic;
static const char *job_error;
static uint32_t worker_stack[4096];
static semaphore_t work, worker_ready;
static queue_t events;
/* USB output must never block polling the robot's slow UART. */
static uint8_t usb_tx[16384];
static unsigned tx_read, tx_write;
static bool tx_overflow;
static void output(void *context, RobotPort port, const char *text,
                   size_t length) {
  (void)context;
  if (port == ROBOT_UART) {
    for (size_t i = 0; i < length; i++)
      uart_putc_raw(uart0, text[i]);
    return;
  }
  if (!stdio_usb_connected())
    return;
  for (size_t i = 0; i < length; i++) {
    unsigned next = (tx_write + 1) % sizeof(usb_tx);
    if (next == tx_read) {
      tx_overflow = true;
      return;
    }
    usb_tx[tx_write] = (uint8_t)text[i];
    tx_write = next;
  }
}
static void drain_usb(void) {
  if (!stdio_usb_connected())
    return;
  unsigned available = tud_cdc_write_available();
  if (!available || tx_read == tx_write)
    return;
  unsigned count =
      tx_write > tx_read ? tx_write - tx_read : sizeof(usb_tx) - tx_read;
  if (count > available)
    count = available;
  unsigned sent = tud_cdc_write(usb_tx + tx_read, count);
  tx_read = (tx_read + sent) % sizeof(usb_tx);
  tud_cdc_write_flush();
  if (tx_read == tx_write && tx_overflow) {
    tx_overflow = false;
    const char *message =
        console.usb_sim ? "E" : "ERR USB output overflow; resend command\r\n";
    output(NULL, ROBOT_USB, message, strlen(message));
  }
}
static bool start(void *context, const RobotVoice *voice,
                  const RobotRule *rules, unsigned count, const char *text,
                  bool phonetic) {
  (void)context;
  if (count > ROBOT_DICTIONARY)
    return false;
  robot_init(&job_robot);
  job_robot.voice = *voice;
  job_count = count;
  memcpy(job_rules, rules, count * sizeof(RobotRule));
  strcpy(job_text, text);
  job_phonetic = phonetic;
  audio_begin();
  sem_release(&work);
  return true;
}
static void stop(void *context) {
  (void)context;
  audio_request_stop();
}
static void volume(void *context, unsigned percent) {
  (void)context;
  audio_set_volume(percent);
}
static bool serial(void *context, unsigned baud, unsigned framing) {
  (void)context;
  uart_tx_wait_blocking(uart0);
  uart_set_baudrate(uart0, baud);
  uart_set_format(uart0, framing ? 7 : 8, 1,
                  framing ? UART_PARITY_EVEN : UART_PARITY_NONE);
  while (uart_is_readable(uart0))
    (void)uart_getc(uart0);
  uart_get_hw(uart0)->rsr = 0;
  return true;
}
static int save(void *context, const RobotBank *b) {
  (void)context;
  return bank_storage_save(b);
}
static bool load(void *context, RobotBank *b, bool *migrated) {
  (void)context;
  return bank_storage_load(b, migrated);
}
static bool erase(void *context) {
  (void)context;
  return bank_storage_erase();
}
static void worker(void) {
  if (!flash_safe_execute_core_init())
    panic("flash safety init failed");
  sem_release(&worker_ready);
  for (;;) {
    sem_acquire_blocking(&work);
    job_error = robot_plan_with_rules(&job_robot, job_rules, job_count,
                                      job_text, job_phonetic, &plan);
    bool ok = false;
    unsigned event;
    if (!job_error) {
      event = ROBOT_STARTED;
      queue_add_blocking(&events, &event);
      ok = robot_render(&plan, audio_sample, NULL);
    }
    audio_finish();
    event = audio_stopped() ? ROBOT_STOPPED : ok ? ROBOT_DONE : ROBOT_FAILED;
    queue_add_blocking(&events, &event);
  }
}
int main(void) {
  stdio_init_all();
  uart_init(uart0, 9600);
  gpio_set_function(ROBOT_UART_TX_PIN, GPIO_FUNC_UART);
  gpio_set_function(ROBOT_UART_RX_PIN, GPIO_FUNC_UART);
  uart_set_hw_flow(uart0, false, false);
  sem_init(&work, 0, 1);
  sem_init(&worker_ready, 0, 1);
  queue_init(&events, sizeof(unsigned), 8);
  bool migrated = false, present = bank_storage_load(&boot, &migrated);
  RobotConsoleOps ops = {NULL,   output, start, stop,  volume,
                         serial, save,   load,  erase, audio_backend()};
  robot_console_init(&console, &ops, &boot, present, migrated);
  audio_init();
  multicore_launch_core1_with_stack(worker, worker_stack, sizeof(worker_stack));
  sem_acquire_blocking(&worker_ready);
  /* Deliberately no UART boot banner or mirrored diagnostics. */
  bool connected = false;
  for (;;) {
    bool usb = stdio_usb_connected();
    if (usb != connected) {
      robot_console_usb_reset(&console);
      tx_read = tx_write = 0;
      tx_overflow = false;
      connected = usb;
    }
    if (!usb)
      tud_cdc_read_flush();
    unsigned event;
    while (queue_try_remove(&events, &event))
      robot_console_event(&console, (RobotEvent)event, job_error,
                          audio_underruns(), audio_clipped());
    for (unsigned i = 0; i < 32 && uart_is_readable(uart0); i++) {
      unsigned data = uart_get_hw(uart0)->dr;
      if (data & 0xf00u) {
        robot_console_uart_error(&console);
        uart_get_hw(uart0)->rsr = 0;
      }
      robot_console_feed(&console, ROBOT_UART, data & 255u);
    }
    for (unsigned i = 0; usb && i < 32 && tud_cdc_available(); i++) {
      int value = tud_cdc_read_char();
      if (value >= 0)
        robot_console_feed(&console, ROBOT_USB, (unsigned)value);
    }
    drain_usb();
    tight_loop_contents();
  }
}
