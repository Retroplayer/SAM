# Preserved 0.4.0 fallback

Exact previously published binaries, preserved before the USB/UART split:

- [I2S UF2](https://github.com/Retroplayer/SAM/raw/refs/heads/main/downloads/v0.4.0/robot-sam-I2S.uf2)
- [PWM UF2](https://github.com/Retroplayer/SAM/raw/refs/heads/main/downloads/v0.4.0/robot-sam-PWM.uf2)
- [Checksums](SHA256SUMS)
- [Archived manual](../../docs/FIRMWARE_MANUAL_0.4.md)

Uses the older verbose USB/UART protocol, fixed UART 115200 8N1, one saved
configuration with 12 rules, and VOLUME 0–400. I2S wiring is LRC GP8, BCLK GP9,
DIN GP10. This version reads legacy settings, not new 0.5.0 bank edits.
The user's confirmed hardware-tested release is [0.3.0](../v0.3.0/README.md).

Notices: [SDK](../SDK-LICENSE.txt), [TinyUSB](../TinyUSB-LICENSE.txt),
[PIO](../../firmware/LICENSE.pio), [SAM reference](../../THIRD_PARTY.md).
