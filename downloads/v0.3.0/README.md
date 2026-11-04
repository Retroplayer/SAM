# Hardware-tested 0.3.0 fallback

These are the exact previously published 0.3.0 binaries, reported working on
the user's hardware before adding flash persistence. They have not been rebuilt.

- [I2S UF2](https://github.com/Retroplayer/SAM/raw/refs/heads/main/downloads/v0.3.0/robot-sam-I2S.uf2)
- [PWM UF2](https://github.com/Retroplayer/SAM/raw/refs/heads/main/downloads/v0.3.0/robot-sam-PWM.uf2)

I2S wiring remains LRC GP8, BCLK GP9, DIN GP10. UART0 is 115200/8N1 on GP0/GP1.
VOLUME 0..400 is supported; flash configuration commands are not. The reported
hardware test concerns I2S; the PWM file is the companion build from that release.

Checksums are in [SHA256SUMS](SHA256SUMS). Applicable notices remain in
[SDK-LICENSE.txt](../SDK-LICENSE.txt), [TinyUSB-LICENSE.txt](../TinyUSB-LICENSE.txt),
[PIO license](../../firmware/LICENSE.pio), and [THIRD_PARTY.md](../../THIRD_PARTY.md).
