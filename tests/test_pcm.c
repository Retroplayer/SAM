#include "robot_pcm.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    assert(robot_volume_key("VoLuMe"));assert(!robot_volume_key("VOLUMES"));
    assert(!robot_volume_key("VOL"));assert(!robot_volume_key(""));
    for (unsigned gain=0;gain<=ROBOT_VOLUME_MAX;gain++) {
        int previous=-32769;
        for (unsigned sample=0;sample<256;sample++) {
            bool clipped;
            int actual=robot_pcm16((uint8_t)sample,gain,&clipped);
            long reference=((long)sample-128)*256*gain/100;
            assert(clipped==(reference>32767 || reference< -32768));
            if (reference>32767) reference=32767;
            if (reference< -32768) reference=-32768;
            assert(actual==reference);assert(actual>=previous);previous=actual;
            if (!gain) assert(actual==0);
            if (gain==100) assert(robot_pcm8((uint8_t)sample,gain,NULL)==sample);
            if (sample==128) {assert(actual==0);assert(!clipped);}
        }
    }
    assert(robot_pcm16(255,400,NULL)==32767);
    assert(robot_pcm16(0,400,NULL)== -32768);
    puts("PCM checks passed: all 256 samples at gains 0..400, silence, unity, clipping, monotonicity.");
}
