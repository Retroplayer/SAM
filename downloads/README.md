# RP2040-Zero development firmware

**[Firmware user manual](../docs/FIRMWARE_MANUAL.md)** — complete setup and command reference.

Download the file for your audio hardware:

- [PWM UF2 — default](https://github.com/Retroplayer/SAM/raw/refs/heads/main/downloads/robot-sam-PWM.uf2)
- [I2S UF2 — external DAC/amplifier](https://github.com/Retroplayer/SAM/raw/refs/heads/main/downloads/robot-sam-I2S.uf2)

I2S wiring: **LRC/WS = GP8, BCLK = GP9, DIN = GP10**.
Version 0.2.1 changes the I2S clock order and defaults; earlier builds used
BCLK GP11 and LRC GP12. Use the wiring for the firmware version you flash.

Both are version 0.4.0 for Waveshare RP2040-Zero, with USB serial and UART0
(115200 baud). Select the output by flashing the corresponding file.

See [wiring, flashing, commands, and build instructions](../firmware/README.md).

**Validation:** cross-compiled for Cortex-M0+, UF2 contents checked against the
compiled binaries, I2S PIO instruction timing simulated, desktop speech tests
passed. **Not yet tested on physical hardware.**

Built with Pico SDK 2.2.0 (commit
`a1438dff1d38bd9c65dbd693f0e5db4b9ae91779`) and xPack GCC 14.2.1-1.1.
Checksums are in [SHA256SUMS](SHA256SUMS).

Version 0.3.0 adds `SET VOLUME 0..400` to both outputs. Default 100 preserves
the old level; try 150 or 200 for boost. Above 100, peaks may clip; STATUS and
completion events report a clipping count. Character selection preserves the
master volume, and autoload restores a saved volume, or boot defaults to 100. No extra GPIO wiring is needed.

The firmware includes the SAM reference engine described in
[THIRD_PARTY.md](../THIRD_PARTY.md), which has unresolved licensing.
PIO attribution and its BSD license are in
[firmware/LICENSE.pio](../firmware/LICENSE.pio).
Pico SDK and TinyUSB notices are included in [SDK-LICENSE.txt](SDK-LICENSE.txt)
and [TinyUSB-LICENSE.txt](TinyUSB-LICENSE.txt).

## Version 0.4.0: saved configuration

Configure your robot and send SAVE_CONFIG while idle. Voice, volume, custom
pronunciations, and AUTOLOAD policy are saved. LOAD_CONFIG restores manually;
DEFAULTS changes RAM only; ERASE_CONFIG removes the saved snapshot. AUTOLOAD
is enabled by default and can be disabled with SET AUTOLOAD 0 followed by save.
See the full manual for the power-cycle test and error replies.

New persistence has native corruption/interrupted-write tests and cross-build
checks; physical flash write and playback-resume behavior still need testing.
The [hardware-tested 0.3.0 fallback](v0.3.0/README.md) is preserved separately.
