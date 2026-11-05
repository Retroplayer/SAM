# RP2040-Zero firmware downloads

**Current: 0.5.0** · [Full user manual](../docs/FIRMWARE_MANUAL.md)

- [I2S UF2](https://github.com/Retroplayer/SAM/raw/refs/heads/main/downloads/robot-sam-I2S.uf2)
- [PWM UF2 — default](https://github.com/Retroplayer/SAM/raw/refs/heads/main/downloads/robot-sam-PWM.uf2)
- [SHA256 checksums](SHA256SUMS)

Open a file on GitHub and choose Download raw file, or use the links above.
I2S wiring stays **LRC GP8, BCLK GP9, DIN GP10**. PWM stays GP14.

## New console design

USB is the line-oriented editor and speech-test connection. Start with HELP,
STATUS, and VOICE LIST. Characters (8 slots), shared pronunciations (64 entries),
serial settings, apply/save/load, and help topics are all edited through USB.
USB simulation uses exact `!~` / `~!` lines to enter/exit while idle, sharing the
byte-fed UART parser and isolated temporary tweaks.

**Breaking UART change:** robot UART now accepts only short speech commands
such as `S Hello!`, `C 1`, `V 200`, `?`, and `!`, each terminated by CR/LF.
Replies are bare A/D/R/B/E bytes with no line endings. There are no mirrored
USB diagnostics, UART editor mode, flash commands, or startup banner.

Factory volume is **118** (100 remains unity); saved volumes are preserved.
Factory UART is 9600 8N1. A valid old 0.4.0 save migrates automatically in RAM,
preserving voice, volume, pronunciations and autoload policy, with UART
115200 8N1. Inspect via USB and save to the new bank format. Supported rates
are 1200–115200, with 8N1 or 7E1; UART APPLY explicitly changes the live link.

## Validation and fallback

Both builds are cross-compiled and pass native parser/storage/audio tests,
UF2 reserved-region checks, and simulated I2S timing. New USB routing, physical
UART framing, and migration still need board testing. The preserved files
below are exact earlier binaries, not rebuilt substitutes:

- [0.4.0 fallback](v0.4.0/README.md)
- [Hardware-tested 0.3.0 fallback](v0.3.0/README.md)

Built with Pico SDK 2.2.0 and xPack GCC 14.2.1-1.1. Read
[THIRD_PARTY.md](../THIRD_PARTY.md), [SDK-LICENSE.txt](SDK-LICENSE.txt),
[TinyUSB-LICENSE.txt](TinyUSB-LICENSE.txt), and
[PIO license](../firmware/LICENSE.pio) for included code notices.
