#ifndef ROBOT_PCM_H
#define ROBOT_PCM_H
#include <stdbool.h>
#include <stdint.h>

#define ROBOT_VOLUME_DEFAULT 100u
#define ROBOT_VOLUME_MAX 400u

static inline bool robot_volume_key(const char *key) {
    const char *name="VOLUME";
    while (*key && *name) {
        char c=*key++;
        if (c>='a' && c<='z') c=(char)(c-'a'+'A');
        if (c!=*name++) return false;
    }
    return !*key && !*name;
}
/* Scale around signed silence, then saturate instead of wrapping. 100 = unity.
 * Use 16-bit precision for I2S, even though the synthesis source is 8-bit. */
static inline int16_t robot_pcm16(uint8_t sample, unsigned volume, bool *clipped) {
    int32_t value=((int32_t)sample-128)*256*(int32_t)volume/100;
    bool limit=value>32767 || value< -32768;
    if (value>32767) value=32767;
    if (value< -32768) value=-32768;
    if (clipped) *clipped=limit;
    return (int16_t)value;
}
static inline uint8_t robot_pcm8(uint8_t sample, unsigned volume, bool *clipped) {
    return (uint8_t)(((int32_t)robot_pcm16(sample,volume,clipped)+32768)/256);
}
#endif
