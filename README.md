# SAM: robot speech for RP2040-Zero

A first working audition prototype for expressive 1980s robot speech. Select a
character once, then send text. A portable C11 planner expands acronyms and
numbers, interprets punctuation and emphasis, and supplies character-dependent
delivery to a SAM reference backend. No network or AI service is used at runtime.

**Current status:** desktop CLI and WAV rendering work. RP2040-Zero firmware now
builds with default PWM/DMA audio or selectable PIO/DMA I2S audio, plus UART and
USB serial commands. Both UF2 variants are available in [downloads](downloads/README.md).
They are cross-compiled and structurally checked, but have not been tested on a
physical board. I2C/SPI command transports and embedded character persistence
are not implemented. Presets are initial tuning candidates, not verified
reproductions of particular toys.

See [firmware setup, wiring, and commands](firmware/README.md).

## Build and hear the characters

### Download the audition files

These recordings speak the same text through three different characters:

- [Companion WAV](demos/companion.wav)
- [Dictator WAV](demos/dictator.wav)
- [Malfunction WAV](demos/malfunction.wav)

On GitHub, open a file and use **Download raw file** to download it.

Spoken text: "Hello, my friend. Are you ready? The CPU is online. You have been warned!"

### Build locally

Prerequisites: a C compiler, Make, Git; Python 3 for audio integration tests.

```sh
cd /workspace/SAM
make reference
make -j2
make test
make demo
```

`make reference` downloads a pinned, unmodified arduino-SAM checkout into ignored
`.deps/`. Read [THIRD_PARTY.md](THIRD_PARTY.md) before incorporating or distributing
that engine. Its licensing is unresolved; this project does not treat
"abandonware" as a license.

The demo renders the **same text** through three characters:

- `build/companion.wav`
- `build/dictator.wav`
- `build/malfunction.wav`

Files are mono, unsigned 8-bit PCM at 22,050 Hz. Use any WAV player to audition
them. A single command can render other text:

```sh
./build/robot-sam --character DICTATOR --wav build/warning.wav \
  'RX-7, remain where you are. Your CPU is under my control!'
```

Use `--plan` to inspect pronunciation, raw SAM parameters, and pauses without
rendering audio:

```sh
./build/robot-sam --plan 'Where are you going? Is the USB ready?'
```

## Define once, then speak

Run `./build/robot-sam --console`, then enter:

```text
CHARACTER COMPANION
PRONOUNCE ROS=ross
SET EXPRESSION 80
SAVE_CHARACTER build/my-robot.profile
SAY Hello, my friend! ROS is ready.
SAY The CPU has detected movement. Are you ready?
STATUS
QUIT
```

In a later session, use `LOAD_CHARACTER build/my-robot.profile` and continue with
`SAY`. Files save the full voice and custom pronunciation dictionary. Loading an
invalid file leaves the current character untouched. These are desktop files;
embedded flash persistence is not implemented yet.

Console `SAY` commands write `speech-001.wav`, `speech-002.wav`, etc. in the current
directory. Rendering is synchronous. There is no command queue or live STOP in
this desktop version. Commands are case sensitive; preset names and setting
names are case insensitive. Command errors produce a nonzero final exit status.

Characters: `CLASSIC`, `COMPANION`, `GUARDIAN`, `ENFORCER`, `DICTATOR`,
`MALFUNCTION`. Selecting a character replaces voice settings while preserving
custom pronunciations. `CLASSIC` disables the added expression and effects, but
still uses text normalization; it is not a claim of bit-exact original emulation.

| Setting | Range | Meaning |
| --- | --- | --- |
| `PITCH` | 0..255 | Raw SAM pitch period; lower generally sounds higher |
| `SPEED` | 0..255 | Raw SAM timing; higher generally sounds slower |
| `MOUTH`, `THROAT` | 0..255 | Original SAM formant transformations |
| `SING` | 0 or 1 | Original singing mode |
| `EXPRESSION` | 0..100 | Added emphasis and stepped phrase contours |
| `METALLIC` | 0..100 | Dry/wet square-wave ring modulation at 140 Hz |
| `ROUGHNESS` | 0..100 | Quantization from 8 down to 3 bits |

The entire raw byte range is exposed for original SAM controls; extreme settings
are experimental and may be unintelligible. Not every parameter combination has
been validated. Voice pitch, timing, and formants remain accessible independently.

Advanced phoneme input bypasses text normalization:

```sh
./build/robot-sam --phonemes --wav build/hello.wav '/HEHLOW'
```

Phonetic console input uses `PHONEMES /HEHLOW`. Original stress digits are passed
through to SAM. See the reference's `.deps/arduino-SAM/PHONES.md` for symbols.

## Automatic delivery and pronunciation

- Yes/no questions end higher; questions starting with who/what/where/when/why/
  how/which/whose end lower. This is a heuristic, not grammatical understanding.
- Mixed-case messages can emphasize uppercase words. Common short words such as
  `WHAT` and `STOP` are kept as words. All-uppercase messages suppress automatic
  acronym guessing and capitalization emphasis; known acronyms still expand.
- Commas, sentence endings, colons, semicolons, and ellipses introduce different
  pauses. Exclamation marks increase the pitch emphasis of a phrase.
- Known initialisms include CPU, USB, AI, I2C, SPI, and LED. NASA, RAM, ROM, laser,
  and radar are pronounced as words. UART is pronounced "you art" by default.
- Unknown 2..4-letter uppercase words in mixed-case messages are spelled out.
  Dictionary overrides take precedence; ambiguous cases need explicit entries.
- Letter/digit designations such as RX-7 are spelled out. Ordinary numbers up to
  9999 are read as cardinal numbers, leading-zero strings as digits, and decimal
  fractions digit by digit. Larger numbers fall back to digits.
- V, Hz, and mA after a numeric token expand to units. Singular forms, broader
  units, dates, fractions, and context-sensitive abbreviations remain future work.
- Dr., Mr., Mrs., and approx. have basic expansions. This is deliberately a small
  dictionary; for example, it does not infer "doctor" versus "drive" from context.

These rules are deterministic. The current contour implementation changes SAM
settings **between speech segments**, sometimes splitting the final word from
its phrase. It is intentionally stepped, but can introduce audible joins. It
does not yet provide phoneme-frame pitch envelopes or new formant controls.

## Implementation and limits

- `src/robot.c`: bounded, heap-free planning; original text is not modified.
- `src/render.c`: reference adapter, static engine state, streaming PCM effects.
- `src/profile.c`: versioned desktop character persistence.
- `src/main.c`: audition CLI and line-oriented command interface.
- `include/robot.h`: transport-independent planning and sample-sink API.

Input is printable ASCII plus tabs, with 255 bytes and 40 words maximum per
request. Unsupported symbols are rejected rather than silently pronounced.
Each rendered segment is at most 95 bytes; direct phoneme input is also limited
to 95 bytes. There are 12 custom pronunciation slots. Custom pronunciations
currently accept spelled-out words, not phonetic dictionary entries.

On the current 64-bit desktop, a character occupies 969 bytes and a plan 4328
bytes. Planning also has bounded temporary stack storage. These are not RP2040
flash, stack, RAM, or real-time measurements. The reference synthesizer is
non-reentrant: only one caller may render at a time. A sink can apply
backpressure; returning false aborts output, although the reference can finish
computing the current segment before returning.

`make test` checks pronunciation, delivery decisions, configuration bounds,
profile persistence, six distinct audio outputs, PCM content, and WAV structure.
`make sanitize` runs planner checks under AddressSanitizer and UBSan. It does not
claim to sanitize the entire third-party engine or validate listening quality.

The hardware milestone now needs a physical RP2040-Zero audition: verify PWM and
I2S waveforms, STOP behavior, and underrun counts. The firmware uses buffered DMA
audio, keeps serial commands responsive on core 0, and performs planning and
synthesis on core 1. I2C and SPI command transports remain future work.
