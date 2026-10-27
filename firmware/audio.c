#include "audio.h"
#include "robot.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"
#include "pico/util/queue.h"
#include <stdatomic.h>

#ifdef ROBOT_AUDIO_I2S
#include "i2s.pio.h"
#endif

#define BLOCK_SAMPLES 256u
#define BLOCK_COUNT 8u

static uint32_t buffers[BLOCK_COUNT][BLOCK_SAMPLES];
static uint32_t silence[BLOCK_SAMPLES];
static queue_t available, ready;
static int channels[2];
static atomic_int scheduled[2];
static atomic_bool cancelled, producing, played;
static atomic_uint gaps;
static atomic_uint outstanding;
/* Producer-only state, used on core 1. */
static int filling=-1;
static unsigned filled;

static uint32_t encode(uint8_t sample) {
#ifdef ROBOT_AUDIO_I2S
    uint16_t pcm=(uint16_t)(int16_t)(((int)sample-128)*256);
    return ((uint32_t)pcm<<16)|pcm;
#else
    return sample;
#endif
}
const char *audio_backend(void) {
#ifdef ROBOT_AUDIO_I2S
    return "I2S";
#else
    return "PWM";
#endif
}

static void dma_irq(void) {
    for (unsigned i=0;i<2;i++) {
        uint32_t mask=1u<<channels[i];
        if (!(dma_hw->ints0&mask)) continue;
        dma_hw->ints0=mask;
        int old=atomic_load(&scheduled[i]);
        if (old>=0) {
            if (!queue_try_add(&available,&old)) panic("audio free queue overflow");
            atomic_fetch_sub(&outstanding,1);
        }
        int next=-1;
        if (atomic_load(&cancelled)) {
            int discarded;
            while (queue_try_remove(&ready,&discarded)) {
                if (!queue_try_add(&available,&discarded)) panic("audio free queue overflow");
                atomic_fetch_sub(&outstanding,1);
            }
        } else if (queue_try_remove(&ready,&next)) {
            atomic_store(&played,true);
        } else if (atomic_load(&producing) && atomic_load(&played)) {
            atomic_fetch_add(&gaps,1);
        }
        atomic_store(&scheduled[i],next);
        dma_channel_set_read_addr(channels[i],next<0?silence:buffers[next],false);
        dma_channel_set_trans_count(channels[i],BLOCK_SAMPLES,false);
    }
}

void audio_init(void) {
    queue_init(&available,sizeof(int),BLOCK_COUNT);
    queue_init(&ready,sizeof(int),BLOCK_COUNT);
    for (int i=0;i<(int)BLOCK_COUNT;i++) queue_add_blocking(&available,&i);
    for (unsigned i=0;i<BLOCK_SAMPLES;i++) silence[i]=encode(128);
    volatile void *destination;
    unsigned request;
#ifdef ROBOT_AUDIO_I2S
    PIO pio=pio0;
    unsigned sm=pio_claim_unused_sm(pio,true);
    unsigned offset=pio_add_program(pio,&robot_i2s_program);
    pio_gpio_init(pio,ROBOT_I2S_DATA_PIN);
    pio_gpio_init(pio,ROBOT_I2S_CLOCK_PIN);
    pio_gpio_init(pio,ROBOT_I2S_CLOCK_PIN+1);
    pio_sm_config config=robot_i2s_program_get_default_config(offset);
    sm_config_set_out_pins(&config,ROBOT_I2S_DATA_PIN,1);
    sm_config_set_sideset_pins(&config,ROBOT_I2S_CLOCK_PIN);
    sm_config_set_out_shift(&config,false,true,32);
    sm_config_set_fifo_join(&config,PIO_FIFO_JOIN_TX);
    sm_config_set_clkdiv(&config,(float)clock_get_hz(clk_sys)/(ROBOT_RATE*64u));
    pio_sm_init(pio,sm,offset,&config);
    pio_sm_set_consecutive_pindirs(pio,sm,ROBOT_I2S_DATA_PIN,1,true);
    pio_sm_set_consecutive_pindirs(pio,sm,ROBOT_I2S_CLOCK_PIN,2,true);
    pio_sm_set_pins(pio,sm,0);
    pio_sm_exec(pio,sm,pio_encode_jmp(offset+robot_i2s_offset_entry_point));
    destination=&pio->txf[sm];
    request=pio_get_dreq(pio,sm,true);
#else
    gpio_set_function(ROBOT_PWM_PIN,GPIO_FUNC_PWM);
    unsigned slice=pwm_gpio_to_slice_num(ROBOT_PWM_PIN);
    pwm_config config=pwm_get_default_config();
    pwm_config_set_wrap(&config,255);
    pwm_init(slice,&config,true);
    pwm_set_gpio_level(ROBOT_PWM_PIN,128);
    /* DMA timer supplies the sample clock; PWM retains its ~488 kHz carrier. */
    int timer=dma_claim_unused_timer(true);
    uint32_t clock=clock_get_hz(clk_sys);
    uint32_t numerator=(uint32_t)((uint64_t)65535*ROBOT_RATE/clock);
    assert(numerator>0 && numerator<=65535);
    uint32_t denominator=(uint32_t)(((uint64_t)clock*numerator+ROBOT_RATE/2)/ROBOT_RATE);
    dma_timer_set_fraction(timer,(uint16_t)numerator,(uint16_t)denominator);
    destination=&pwm_hw->slice[slice].cc;
    request=DREQ_DMA_TIMER0+(unsigned)timer;
#endif
    channels[0]=dma_claim_unused_channel(true);
    channels[1]=dma_claim_unused_channel(true);
    for (unsigned i=0;i<2;i++) {
        atomic_store(&scheduled[i],-1);
        dma_channel_config config=dma_channel_get_default_config(channels[i]);
        channel_config_set_transfer_data_size(&config,DMA_SIZE_32);
        channel_config_set_read_increment(&config,true);
        channel_config_set_write_increment(&config,false);
        channel_config_set_dreq(&config,request);
        channel_config_set_chain_to(&config,channels[1-i]);
        dma_channel_configure(channels[i],&config,destination,silence,BLOCK_SAMPLES,false);
        dma_channel_set_irq0_enabled(channels[i],true);
    }
    irq_set_exclusive_handler(DMA_IRQ_0,dma_irq);
    irq_set_enabled(DMA_IRQ_0,true);
    dma_start_channel_mask(1u<<channels[0]);
#ifdef ROBOT_AUDIO_I2S
    pio_sm_set_enabled(pio,sm,true);
#endif
}

void audio_begin(void) {
    assert(filling<0);
    atomic_store(&cancelled,false);
    atomic_store(&played,false);
    atomic_store(&gaps,0);
    atomic_store(&producing,true);
}
bool audio_sample(void *context, uint8_t sample) {
    (void)context;
    if (atomic_load(&cancelled)) return false;
    if (filling<0) {
        while (!queue_try_remove(&available,&filling)) {
            if (atomic_load(&cancelled)) return false;
            tight_loop_contents();
        }
        filled=0;
    }
    buffers[filling][filled++]=encode(sample);
    if (filled==BLOCK_SAMPLES) {
        atomic_fetch_add(&outstanding,1);
        queue_add_blocking(&ready,&filling);
        filling=-1;
    }
    return true;
}
void audio_finish(void) {
    if (filling>=0) {
        if (atomic_load(&cancelled)) queue_add_blocking(&available,&filling);
        else {
            while (filled<BLOCK_SAMPLES) buffers[filling][filled++]=encode(128);
            atomic_fetch_add(&outstanding,1);
            queue_add_blocking(&ready,&filling);
        }
        filling=-1;
    }
    atomic_store(&producing,false);
    while (atomic_load(&outstanding))
        tight_loop_contents();
}
void audio_request_stop(void) {atomic_store(&cancelled,true);}
bool audio_stopped(void) {return atomic_load(&cancelled);}
uint32_t audio_underruns(void) {return atomic_load(&gaps);}
