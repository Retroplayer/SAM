# RP2040-Zero firmware

Two **build-time** audio choices share the same speech engine and commands:

- **PWM**, the default: hardware PWM on GP14, fed by timer-paced DMA.
- **I2S**: PIO-generated standard Philips I2S, fed by DMA; external DAC or digital
  amplifier required. No MCLK, device register setup, or chip-specific driver.

Download the prebuilt UF2 files from [downloads](../downloads/README.md).
These are development builds: compilation, UF2 structure, and simulated I2S bit
timing have been checked; physical-board playback and electrical timing have not.

## Flash and connect

1. Hold BOOT on the RP2040-Zero while connecting USB (or use its BOOT/RESET
   sequence) to enter the `RPI-RP2` USB mass-storage bootloader.
2. Copy the chosen UF2 onto `RPI-RP2`. It reboots into the firmware.
3. Open the USB CDC serial port, or connect a 3.3 V UART host at **115200, 8N1**.
   USB serial does not require a particular baud-rate setting.
4. Send newline-terminated commands. If the boot banner was missed, send `STATUS`.

The board defaults to `COMPANION` after reset. There is no automatic boot speech.

## Default wiring

GPIO numbers below are RP2040 GPIO numbers, not physical header pin numbers.

| Connection | GPIO | Direction |
| --- | --- | --- |
| UART TX | GP0 | Board to host RX |
| UART RX | GP1 | Host TX to board |
| PWM audio | GP14 | Output; PWM build only |
| I2S DIN/data | GP10 | Output to DAC/amplifier DIN |
| I2S BCLK | GP9 | Output to DAC/amplifier BCLK |
| I2S LRCLK/WS | GP8 | Output to DAC/amplifier LRC/WS |

Connect common ground. UART and I2S signals use 3.3 V logic.
The onboard GP16 RGB LED is not used.

### PWM

GP14 is a switching signal, **not a speaker output**. Feed it through a low-pass
filter and appropriate AC coupling into an analog amplifier, then the speaker.
For example, a 2.2 kΩ series resistor and 10 nF capacitor to ground provide a
first-order filter around 7.2 kHz. The amplifier input may need additional
filtering, attenuation, or coupling depending on its circuit. Start with low
amplifier gain. Do not connect a speaker directly to a GPIO.

At the SDK's default 125 MHz system clock, PWM has a carrier of about 488 kHz,
with an independent DMA sample clock near 22,050 Hz. Idle output is 50% duty
cycle, corresponding to unsigned PCM silence at 128. This intentionally retains
SAM's 8-bit amplitude character.

### I2S

A **MAX98357A I2S amplifier module** is a suitable initial candidate. Connect
GP10 to DIN, GP9 to BCLK, and GP8 to LRC; power the module according to its
specification and connect its speaker outputs as specified by the module.

Output is master-mode Philips I2S, MSB first, **16-bit signed samples in two
16-bit slots**, nominally 22,050 frames/s. The mono sample is duplicated into
both slots; BCLK is nominally 705.6 kHz. The eight-bit SAM sample is expanded to
16 bits without interpolation. Fractional PIO division introduces a small clock
rounding error; actual timing still needs measurement on hardware.

The backend does not depend on MAX98357A. Other DACs/amplifiers can work if they
accept this format and rate, work without MCLK, and need no register setup.
Devices requiring MCLK, other slot widths, or I2C configuration need additional
support. Changing backend currently means flashing the other UF2, not sending
a runtime command.

## Commands

```text
CHARACTER COMPANION
PRONOUNCE ROS=ross
SET EXPRESSION 80
SAY Hello, my friend! The CPU is ready.
STATUS
STOP
CHARACTER DICTATOR
SAY Remain where you are. ROS is now under my control!
```

Characters, raw SAM settings, effects, pronunciation rules, and text limits are
the same as the [desktop prototype](../README.md). `PHONEMES /HEHLOW` bypasses
normalization. `HELP` lists commands.

- `OK` confirms a character, setting, pronunciation, or STOP command.
- `ACCEPTED` acknowledges a speech request; worker-side planning can still reject
  its content with `ERR ...`.
- `STARTED` means synthesis has started, not that sound has already left the DAC.
- `DONE underruns=N` is emitted after all queued audio blocks have played.
- A second speech request while busy gets `ERR busy`; no speech queue is exposed.
- `STOP` stops further production and drops queued buffers. Already scheduled
  DMA audio can take up to roughly two 256-sample blocks (~23 ms) to finish,
  plus peripheral buffering. `STOPPED` follows once the worker has returned and
  playback buffers are drained. This latency is a design estimate, not measured.
- `STATUS` includes `IDLE`/`BUSY`, selected audio backend, settings, and underruns.
- Character changes during speech apply to the next request; each request has
  its own snapshot of the voice and pronunciation dictionary.

Replies/events are mirrored to USB and UART. Each has its own receive buffer,
so simultaneous input does not concatenate bytes across transports. Use one
controlling host at a time to avoid conflicting commands. Both CRLF and LF work.
Settings persist in RAM until reset; desktop file-based `SAVE_CHARACTER` and
`LOAD_CHARACTER` are not available in this firmware.

## Build

Prerequisites: Git, CMake 3.20+, Make or Ninja, a host C/C++ compiler, Python 3,
and an `arm-none-eabi` C/C++ toolchain with newlib on PATH. Validated here with
xPack GCC 14.2.1-1.1, Pico SDK 2.2.0, and CMake 4.4.4.

From the repository root:

```sh
make reference
make sdk
make firmware-pwm
make firmware-i2s
make firmware-test
```

The SDK helper pins commit `a1438dff1d38bd9c65dbd693f0e5db4b9ae91779`
(2.2.0) and initializes its pinned TinyUSB submodule. An existing SDK can instead
be supplied through `PICO_SDK_PATH`. Pico SDK fetches/builds its matching picotool
to produce UF2 files, so first configuration also needs GitHub access.

Output files:

```text
build/pwm/robot-sam-PWM.uf2
build/i2s/robot-sam-I2S.uf2
```

Plain CMake selects PWM when `AUDIO_OUTPUT` is omitted in a fresh build directory:

```sh
cmake -S firmware -B build/custom -DCMAKE_BUILD_TYPE=Release
cmake --build build/custom -j2
```

To select I2S and customize pins:

```sh
cmake -S firmware -B build/custom-i2s -DCMAKE_BUILD_TYPE=Release \
  -DAUDIO_OUTPUT=I2S -DROBOT_I2S_DATA_PIN=10 -DROBOT_I2S_CLOCK_PIN=9
cmake --build build/custom-i2s -j2
```

Other CMake settings: `ROBOT_PWM_PIN`, `ROBOT_UART_TX_PIN`, `ROBOT_UART_RX_PIN`.
PWM must use an even GPIO (channel A). I2S LRCLK is always BCLK GPIO - 1.
Configuration rejects invalid modes, GPIO ranges, overlapping active pins,
and pins incompatible with UART0. Check the board pinout before assigning
alternative GPIOs; some RP2040 GPIOs are not exposed or have onboard uses.

## Buffering and validation

Core 0 handles USB/UART, DMA completion interrupts, and a 4 KiB stack. Core 1
uses an explicit 16 KiB SRAM stack for planning and synthesis, avoiding the
RP2040's small default scratch-bank stack. Eight fixed 256-frame buffers provide
backpressure. Chained DMA channels keep playback running; the completion IRQ
selects another ready block or a silence block. Silence also runs while idle,
so I2S clocks do not intentionally stop between utterances.

Underruns count silence blocks scheduled after playback has begun while the
producer is still active. They reset per request and are reported in `STATUS`
and completion events. They do not measure analog quality or I2S FIFO faults.

`make firmware-test` validates both UF2 files against their compiled binaries,
checks the RP2040 family/address/block metadata, and simulates the shipped PIO
instructions to check MSB order and the one-bit I2S word-select delay. Desktop
speech regression tests remain available through `make test`. Configuration
checks also verify that five invalid backend/pin assignments are rejected.

Before relying on hardware, verify both outputs with a board and amplifier;
check BCLK/LRCLK with a scope or logic analyzer, audition speech, exercise STOP,
and monitor underruns. I2C/SPI command transports and flash profile storage are
separate future features.
