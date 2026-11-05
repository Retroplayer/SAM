# RP2040-Zero firmware 0.5.0

**[User manual](../docs/FIRMWARE_MANUAL.md)** · **[UF2 downloads](../downloads/README.md)**

## Outputs and connections

PWM is the build default: GP14, hardware PWM carrier with timer-paced DMA.
Use a filter and analog amplifier; never drive a speaker directly.
I2S uses PIO/DMA: LRC GP8, BCLK GP9, DIN GP10. It emits duplicated mono,
16-bit stereo slots at nominal 22,050 frames/s, with no MCLK or DAC register
configuration. MAX98357A is a suitable external amplifier. GPIOs are configurable
at build time; LRC must remain BCLK minus 1, and PWM uses an even GPIO.

UART0 is on GP0 TX/GP1 RX, 3.3 V logic. Its factory configuration is 9600 8N1;
valid legacy migration preserves 115200 8N1. USB provides all editing/saving,
speech auditioning, help, and UART simulation. UART provides only compact
speech commands and single-character feedback. Responses are never mirrored.
There is no UART boot banner. USB defaults to editor; send HELP or STATUS.

## Build

Prerequisites: Git, CMake 3.20+, Make or Ninja, Python 3, a host C/C++ compiler,
and an arm-none-eabi C/C++ compiler with newlib. Validated using xPack GCC
14.2.1-1.1 and Pico SDK 2.2.0, pinned by tools/fetch-sdk.sh.

```sh
make reference
make sdk
make test
make firmware-pwm
make firmware-i2s
make firmware-test
```

Results: `build/pwm/robot-sam-PWM.uf2`, `build/i2s/robot-sam-I2S.uf2`.
SDK initialization fetches matching picotool to build UF2 files. Set
PICO_SDK_PATH to use an existing SDK.

For an explicit custom build:

```sh
cmake -S firmware -B build/custom-i2s -DCMAKE_BUILD_TYPE=Release \
  -DAUDIO_OUTPUT=I2S -DROBOT_I2S_DATA_PIN=10 -DROBOT_I2S_CLOCK_PIN=9
cmake --build build/custom-i2s -j2
```

Other GPIO options: ROBOT_PWM_PIN, ROBOT_UART_TX_PIN, ROBOT_UART_RX_PIN.
UART0 GPIO assignments, ranges, and active pin overlaps are checked during
configuration. Actual baud/framing are configured through the USB editor,
not a compiler define. The USB 1200-baud reset shortcut is disabled.

## Implementation

- `src/console.c`: portable USB editor, shared UART/simulation byte parser,
  ownership, workspace/apply rules, compact events, and USB-only help.
- `src/bank.c`: eight characters, 64 dictionary rules, validation and schema-2 codec.
- `firmware/main.c`: GPIO/UART/USB adapters, deferred nonblocking USB output,
  16 KiB USB TX queue, synthesis job snapshots and event delivery.
- `src/robot.c`: pronunciation/planning; extended dictionary passed separately
  so the existing native utility and legacy codec retain their 12-rule layout.
- `firmware/bank_storage.c`: new journal and read-only legacy migration.
- `firmware/storage.c`: retained legacy adapter tested for 0.4.0 compatibility;
  not linked into the new firmware.
- `firmware/audio.c`: eight 256-frame buffers, chained DMA, idle silence,
  master digital volume, counters, and flash pause/resume.

Core 0 handles both interfaces and DMA IRQs. USB text is queued, then drained
without blocking UART polling. Core 1 has an explicit 16 KiB SRAM stack for
planning/synthesis and registers for Pico SDK flash-safe lockout. It receives
copies of the selected voice, dictionary, and input; editing cannot mutate a
running job. The engine remains non-reentrant.

All input is 7-bit ASCII. 7E1 uses hardware 7 data bits/even parity; invalid
UART receive flags mark the current line invalid. Both parsers require CR/LF,
including single-character commands. No interactive key editing is implemented:
the USB terminal edits locally before transmitting complete lines.

## Flash layout and migration

The target is the Zero's 2 MiB flash. The linker reserves its final **24 KiB**:

| Offset | Size | Use |
| --- | --- | --- |
| 0x1FA000 | 8 KiB | New bank journal slot A |
| 0x1FC000 | 8 KiB | New bank journal slot B |
| 0x1FE000 | 4 KiB | Legacy 0.4.0 slot A |
| 0x1FF000 | 4 KiB | Legacy 0.4.0 slot B |

Schema-2 records are 6144 bytes, including a sequence, CRC32, and commit marker.
They include global settings, eight fixed character slots, and 64 dictionary
entries. Validation occurs before decode mutates caller state. There is no
large record-sized automatic stack allocation. Saves alternate 8 KiB slots,
skip identical payloads, and verify readback. ERASE_CONFIG clears all 24 KiB.

Without a valid new bank, a valid legacy record is read and converted in RAM.
The old voice becomes ROBOT slot 1, and all old volume/rules/autoload values
are retained. Serial defaults for migrated configurations are 115200 8N1.
Legacy sectors are not modified during migration or normal new saves.
Normal UF2 images exclude all reserved sectors. Old fallback firmware reads
legacy settings rather than later bank edits.

Flash operations require idle, stop/restart DMA/PIO, and use the SDK flash-safe
API to park core 1 and mask unsafe interrupts during erase/program. USB/UART
service pauses during the operation; hosts must await acknowledgement.

## Verification

Native tests execute the same command core as firmware. They cover buffered
lines, fragmented byte input, compact response bytes, error recovery, ownership,
USB/UART isolation, workspace editing, character/dictionary operations, UART
settings, simulation and help. Storage/codec tests include legacy migration,
partial programming, corruption, sequence rollover, no-op writes, audio guards,
and erasure without old data reappearing. Existing speech/audio tests remain.

UF2 tests compare actual flash payloads with their binaries, exclude reserved
sectors, and simulate PIO bit order/word-select delay. These do not verify real
UART voltages/parity, Windows CDC behavior, physical power-cycle recovery,
DMA timing, or amplifier sound. Follow the manual's acceptance test on hardware.
