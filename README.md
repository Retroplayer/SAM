# Robot SAM for RP2040-Zero

Expressive 1980s robot speech: configure a character once, then send text.
SAM is the synthesis foundation; pronunciation, delivery, and character
controls can evolve beyond original-machine compatibility.

**[Complete firmware manual](docs/FIRMWARE_MANUAL.md)** ·
**[Download firmware](downloads/README.md)** ·
**[Build/development notes](firmware/README.md)**

## Firmware 0.5.0

- **USB editor:** line-oriented character/dictionary editing, direct speech
  audition, diagnostics, and topic-based HELP. Use an Arduino-style terminal
  that edits a line locally before sending it.
- **Robot UART:** 7-bit ASCII short commands with bare A/D/R/B/E replies.
  Configurable 1200–115200 baud, 8N1 or 7E1. No mirrored USB chatter or boot banner.
- **USB UART simulation:** same byte-fed compact parser and feedback, isolated
  temporary tweaks. Enter with a complete `!~` line; exit idle with `~!`.
- Eight character slots, 64 shared pronunciation overrides, explicit APPLY
  and SAVE_CONFIG, automatic boot restoration, and non-destructive 0.4.0 migration.
- PWM default or I2S builds, digital VOLUME 0–400 (factory default 118; unity 100), clipping/underrun diagnostics.

**Breaking UART change:** old verbose UART commands no longer apply. On USB,
start with HELP, STATUS, and VOICE LIST. A new factory setup uses UART 9600 8N1;
a migrated 0.4.0 snapshot keeps 115200 8N1. Use UART SHOW to inspect actual settings.

I2S wiring: **LRC GP8, BCLK GP9, DIN GP10**, common ground, and suitable amplifier
power. PWM uses GP14 through a filter/analog amplifier. The onboard GP16 RGB
LED is unused. No amplifier GAIN or SD connection is required for digital volume.

Both UF2s are built and automatically checked. New console/UART/migration
behavior still needs physical hardware validation. Previous builds are kept in
[downloads/v0.4.0](downloads/v0.4.0/README.md) and the
[hardware-tested 0.3.0 fallback](downloads/v0.3.0/README.md).

## Native WAV auditions

The separate desktop utility remains available. It predates the new firmware
editor and uses its own console/file-profile commands.

```sh
make reference
make -j2
make test
make demo
./build/robot-sam --help
./build/robot-sam --character DICTATOR --wav build/warning.wav 'You have been warned!'
```

Pre-recorded auditions: [companion](demos/companion.wav),
[dictator](demos/dictator.wav), [malfunction](demos/malfunction.wav), and
[Cylon-inspired original-SAM parameters](demos/cylon-sam.wav).

Read [THIRD_PARTY.md](THIRD_PARTY.md) for reference-engine licensing and notices.
