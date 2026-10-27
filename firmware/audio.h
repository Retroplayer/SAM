#ifndef ROBOT_AUDIO_H
#define ROBOT_AUDIO_H
#include <stdbool.h>
#include <stdint.h>

/* Core 0 initializes/begins/requests STOP. Core 1 owns sample/finish. */
void audio_init(void);
const char *audio_backend(void);
void audio_begin(void);
bool audio_sample(void *context, uint8_t sample);
void audio_finish(void);
void audio_request_stop(void);
bool audio_stopped(void);
uint32_t audio_underruns(void);
#endif
