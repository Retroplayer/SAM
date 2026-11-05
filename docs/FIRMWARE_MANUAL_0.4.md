# Robot SAM firmware user manual

> Archived for firmware 0.4.0. [Current manual](FIRMWARE_MANUAL.md).

**Firmware version: 0.4.0 · Board: Waveshare RP2040-Zero**

Robot SAM converts text into deliberately electronic, 1980s-style speech.
Select a character or configure its voice once, then send ordinary text.
Speech generation runs locally; no network connection or external computer
is needed after a robot controller has supplied the text.

This manual describes the firmware currently supplied in this repository.
Desktop-only commands and proposed features are identified separately.

## Contents

1. [Quick start](#1-quick-start)
2. [Hardware, power, and audio wiring](#2-hardware-power-and-audio-wiring)
3. [Flashing and updating](#3-flashing-and-updating)
4. [USB and UART connections](#4-usb-and-uart-connections)
5. [Command reference](#5-command-reference)
6. [Voice settings and presets](#6-voice-settings-and-presets)
7. [Pronunciation and automatic delivery](#7-pronunciation-and-automatic-delivery)
8. [Direct phoneme speech](#8-direct-phoneme-speech)
9. [Recipes](#9-recipes)
10. [Replies and host-controller operation](#10-replies-and-host-controller-operation)
11. [Limits and errors](#11-limits-and-errors)
12. [Troubleshooting](#12-troubleshooting)
13. [Building and checking the firmware](#13-building-and-checking-the-firmware)
14. [Current omissions](#14-current-omissions)

## 1. Quick start

For the MAX98357A module discussed with this project:

1. Wire **LRC to GP8, BCLK to GP9, and DIN to GP10**. Connect ground and
   appropriate amplifier power as described below.
2. Connect a speaker between the amplifier's two speaker terminals.
3. Flash the [I2S UF2](https://github.com/Retroplayer/SAM/raw/refs/heads/main/downloads/v0.4.0/robot-sam-I2S.uf2).
4. Open the board's USB serial port in a serial terminal. Choose a newline
   or CRLF line ending. Enable local echo if you want to see what you type.
5. Send each of these as a separate line:

```text
STATUS
CHARACTER COMPANION
SAY Hello, my friend!
```

`STATUS` should show `audio=I2S`. `CHARACTER` replies `OK`. Speech produces:

```text
ACCEPTED
STARTED
DONE underruns=0 clipped=0
```

Wait for `DONE` before sending another speech request. If the firmware reports
`ERR`, use the error reference below. Use SAVE_CONFIG after tuning to restore the setup automatically on later boots.
Changes remain temporary until explicitly saved.

## 2. Hardware, power, and audio wiring

All pin references are **GPIO numbers**, not header position numbers.

### I2S build: MAX98357A

| RP2040-Zero connection | MAX98357A connection | Purpose |
| --- | --- | --- |
| GP8 | LRC / LRCLK / WS | Audio word/channel clock |
| GP9 | BCLK | Audio bit clock |
| GP10 | DIN | Digital audio data |
| GND | GND | Common ground |
| Suitable supply, normally 5 V | VIN | Amplifier power |
| No connection required | GAIN | Module's default hardware gain |
| No connection required | SD | Module's default enabled/channel setting |

The pictured module is marked **MAX98357A**, **2.5–5.5 V**, **default 9 dB
gain**, and **(L+R)/2 output**. Its header order is LRC, BCLK, DIN, GAIN, SD,
GND, VIN. Follow the printed labels when orienting the board.

The firmware sends the same mono sample on both I2S channels, so the module's
left/right averaging does not change the intended speech.

Use a suitable **4–8 ohm speaker** within the module's specifications.
The speaker connects across the amplifier's `+` and `−` outputs. Neither
speaker output is ground: this is a bridge-output amplifier. Do not connect
either speaker terminal to Pico ground or a grounded audio input.

### Power

The MAX98357A accepts 2.5–5.5 V. A 5 V supply generally provides more available
speaker power than 3.3 V. Its digital inputs still use the Pico's 3.3 V signals.
The amplifier's advertised 3 W generally refers to a 4 ohm load at 5 V under
specified distortion conditions. An 8 ohm speaker is valid but usually allows
nearer 1–2 W maximum. A speaker's 3 W marking is its power-handling rating,
not a promise of loudness; sensitivity and mounting also matter.

On the Waveshare RP2040-Zero, the **5V pin is USB VBUS** and is powered while
USB is connected. It can supply the amplifier if the USB source and cable
can provide sufficient current for both boards. Speaker current can be much
larger than the Pico's own current. Distortion or resets during loud speech
can indicate inadequate power.

Do not use the Pico's 3.3 V regulator as the normal speaker-amplifier supply.
Do not connect an external 5 V supply and USB VBUS together without appropriate
power isolation. For clone boards, confirm their power wiring before relying
on the 5V pin arrangement.

### Gain, volume, and shutdown

**Version 0.3.0 adds digital volume and boost on both I2S and PWM:**

```text
SET VOLUME 200
SAY All systems are operational.
```

| VOLUME | Result |
| --- | --- |
| 0 | Digital mute |
| 50 | Half the original sample amplitude |
| 100 | Original 0.2.1 output level; boot default |
| 200 | 2× sample amplitude, approximately +6 dB before clipping |
| 400 | Maximum 4× amplitude boost, approximately +12 dB before clipping |

The range is 0–400 inclusive, in percent of the original amplitude. This is
not a watts setting or a percentage of the amplifier's rated power. Original
SAM output already reaches full-scale peaks, so boosting increases average
level by clipping some peaks. Start at 150 or 200, then compare. If distortion
is excessive, reduce the setting. Samples saturate rather than wrapping.

Volume is a **master output setting**, independent of character presets.
CHARACTER changes do not reset it. It can change during speech, but buffered
samples keep their previous level: allow roughly 0.1 second for already
prepared audio. On reboot it is restored from a valid saved configuration if autoload is
enabled; otherwise it starts at 100. It is not stored in desktop character
profile files. STATUS reports `volume` and `clipped`; DONE/STOPPED
also report `clipped`, counting generated samples limited at full scale during
the current/last request, including samples later discarded by STOP.

The control is entirely digital: **no GAIN-pin or GPIO11 connection is needed**.
`SET GAIN` is not supported. There is no firmware-controlled SD/mute pin.
STOP stops speech; it does not shut down the amplifier.

Leave the module's GAIN and SD pins at their defaults for initial testing.
GPIO11 is not used for amplifier gain by the current firmware. Connecting
GAIN to it does not create a volume control.

The MAX98357A GAIN pin selects discrete hardware gain settings:

| GAIN connection | Nominal gain |
| --- | --- |
| 100 kΩ resistor to GND | 15 dB |
| Directly to GND | 12 dB |
| Unconnected | 9 dB |
| Directly to VIN | 6 dB |
| 100 kΩ resistor to VIN | 3 dB |

Check for existing module resistors before changing gain. Maximum gain can
cause clipping; it is not necessarily maximum clean output. GAIN is not an
analog potentiometer input. Do not insert an ordinary volume potentiometer
between the amplifier and speaker.

SD combines shutdown and channel selection. It is not connected to the Pico
in this design. Do not assume its pulled-up voltage is safe for a Pico GPIO
when the amplifier uses 5 V; a future GPIO shutdown interface needs suitable
electrical interfacing.

### PWM build

The [PWM UF2](https://github.com/Retroplayer/SAM/raw/refs/heads/main/downloads/v0.4.0/robot-sam-PWM.uf2)
uses **GP14** instead of the I2S connections. GP14 needs a low-pass filter and
an appropriate analog amplifier. It cannot drive a speaker directly, and
it cannot drive the MAX98357A's digital DIN input as an analog signal.

One starting filter is a 2.2 kΩ series resistor with a 10 nF capacitor from
the filtered node to ground, giving a cutoff around 7.2 kHz. Coupling,
attenuation, and additional filtering depend on the analog amplifier input.
PWM idle is 50% duty cycle, so account for its DC level.

At the default 125 MHz system clock, the PWM carrier is approximately
488 kHz. Speech samples are paced independently near 22,050 samples/s.

## 3. Flashing and updating

1. Download the UF2 for your audio hardware from [downloads](../downloads/v0.4.0/README.md).
2. Hold BOOT while connecting USB, or hold BOOT while pressing and releasing
   RESET, then release BOOT.
3. The board should appear as a drive named `RPI-RP2`.
4. Copy the UF2 file onto that drive. The board reboots automatically.
5. Open its serial port and send `STATUS`.

A UF2 is copied to the bootloader drive; it is not sent through the serial
terminal. Re-enter BOOT mode whenever you need to replace the firmware.
Checksums for the repository's downloads are in [SHA256SUMS](../downloads/v0.4.0/SHA256SUMS).

The boot message is:

```text
READY robot-sam 0.4.0 audio=I2S rate=22050
```

The PWM build reports `audio=PWM`. USB may connect after the banner was sent;
missing the banner does not mean startup failed. `STATUS` works afterward.

**Pin-version difference:** 0.2.1 uses LRC GP8, BCLK GP9, DIN GP10.
The earlier 0.2.0 I2S build used BCLK GP11 and LRC GP12. Match wiring to firmware.

## 4. USB and UART connections

### USB serial

Use a data-capable USB cable. Select the new serial port in PuTTY, Tera Term,
Arduino Serial Monitor, or another serial terminal. Names vary by computer,
such as `COM5`, `/dev/ttyACM0`, or `/dev/cu.usbmodem...`.

Selecting 115200 baud is convenient, but USB CDC does not use that setting as
a physical UART bit rate. Set the terminal to send **LF, CR, or CRLF** when
you submit a command. Disable automatic periodic transmissions.

The device does not echo typed characters. Local echo is a terminal option.
Backspace and terminal escape sequences are not interpreted as editing:
use a terminal's line-entry mode to prepare/correct a line before sending it.
An unsupported control byte can cause `ERR command too long` for that line.

### Hardware UART

| Setting | Value |
| --- | --- |
| Interface | UART0, 3.3 V TTL logic |
| Baud | **115200**, fixed in this version |
| Format | 8 data bits, no parity, 1 stop bit: **8N1** |
| Flow control | No RTS/CTS |
| GP0 | Pico TX → host RX |
| GP1 | Pico RX ← host TX |
| GND | Common with host |

Do not connect a true RS-232 voltage-level port directly. Use an appropriate
level converter for RS-232 or a host whose TX is 5 V logic. Ensure any USB-UART
adapter's signal voltage is 3.3 V, not merely its power-output voltage.

USB and UART are both active. Each has a separate input line buffer, and
replies/events are mirrored to both outputs. Use one controlling host at a
time: there are no host addresses, request IDs, or ownership locks.

Slow UART rates, compact command aliases, and a BUSY pin are design goals,
**not features in 0.4.0**. An old robot limited to 1200–9600 baud will need a
future firmware update or an intermediate controller.

## 5. Command reference

Send **one command per line**, without Markdown fences or enclosing quotation
marks. The examples show what to send, not text that should be prefixed with
a prompt. Command words must be uppercase. Parameter names and character names
are case insensitive. Avoid leading indentation and trailing spaces.

### `HELP`

Lists the available command forms. Does not change settings.

### `STATUS`

Returns current state and configuration, for example after a fresh I2S boot:

```text
IDLE audio=I2S underruns=0 volume=100 clipped=0 pitch=58 speed=70 mouth=145 throat=135 expression=65 metallic=0 roughness=0 sing=0 rules=0 saved=0 autoload=1
```

`BUSY` replaces `IDLE` during a request. Settings displayed are the current
configuration for future requests; a running request retains its own snapshot.
`rules` is the number of custom pronunciation entries, not built-in entries.
`saved=1` means a valid snapshot exists in flash; it does not mean current RAM
settings match that snapshot. `autoload` is the current RAM boot-policy setting;
changes to that policy take effect at the next boot only after SAVE_CONFIG.

### `CHARACTER name`

Loads a built-in voice preset:

```text
CHARACTER COMPANION
```

Replies `OK` or `ERR unknown character`. Loading a preset replaces **all eight
voice settings**, including effects, expression, and singing mode. It preserves
custom pronunciation entries and the separate master VOLUME setting. There is no user-defined preset-name storage yet.

### `SET parameter value`

Changes one voice setting:

```text
SET PITCH 93
SET SING 0
```

Use a nonnegative decimal integer in the parameter's range. Replies `OK` or
`ERR invalid setting`. Parameters and ranges are in section 6. Voice settings
can change while busy but apply only to subsequent requests. Master VOLUME
also affects newly generated samples in the running request, after buffering.

### `PRONOUNCE term=replacement words`

Adds or replaces a whole-token pronunciation override:

```text
PRONOUNCE operational=opperational
PRONOUNCE identify=I dentify
```

Replies `OK` or `ERR invalid pronunciation`. Do not put spaces around `=`.
The term is case insensitive; the replacement can contain multiple words.
Details and limits are in section 7.

### `SAY text`

Normal text-to-speech:

```text
SAY Stop! Identify yourself immediately!
```

The module normalizes the text, applies pronunciation overrides, plans
delivery, and synthesizes it using the current character snapshot. The text
does not need surrounding quotes. Wait for the terminal completion event
before sending the next utterance.

### `PHONEMES symbols`

Speaks SAM phonetic notation directly:

```text
PHONEMES /HEHLOW
```

This bypasses spelling rules, dictionary expansion, and the automatic text
delivery planner. It retains current SAM voice settings and audio effects.
Use section 8 for notation and limitations.

### `STOP`

If busy, replies `OK stopping`, cancels production, and discards ready audio
buffers. A later `STOPPED underruns=N clipped=M` confirms completion of the stop.
If already idle, replies `OK idle` with no later STOPPED event required.

Some audio is already scheduled for DMA playback. It can take approximately
two 256-sample blocks (about 23 ms), plus peripheral buffering, to finish.
The synthesizer can finish computing its current segment before returning.
These are design expectations, not a measured end-to-end stop guarantee.
Wait for `STOPPED` before sending replacement speech.

### Flash configuration commands (0.4.0)

There is one saved robot configuration, including all eight voice settings,
master volume, the custom pronunciation dictionary, and the autoload policy.
There are no named slots yet. The audio backend and GPIO wiring are still
selected by the flashed firmware, not by the saved configuration.

All four commands below require IDLE; otherwise they return `ERR busy`.

| Command | Result |
| --- | --- |
| `SAVE_CONFIG` | Store current settings; replies `OK saved`, or `OK unchanged` if already identical |
| `LOAD_CONFIG` | Restore the saved settings, even with autoload disabled; replies `OK loaded` |
| `ERASE_CONFIG` | Remove both saved records; leave current RAM settings unchanged; replies `OK erased` |
| `DEFAULTS` | Restore COMPANION, volume 100, no custom rules, and AUTOLOAD 1 in RAM; leave flash unchanged; replies `OK defaults` |

`LOAD_CONFIG` returns `ERR no saved configuration` if neither stored record is
valid. Save/erase failures return `ERR flash save failed` or `ERR flash erase
failed`. If a write fails, do not assume the new settings were saved; use
LOAD_CONFIG/STATUS to inspect the recoverable state. If the device stops
responding, reboot and check STATUS before retrying.

Control the next boot with:

```text
SET AUTOLOAD 1
SAVE_CONFIG
```

AUTOLOAD accepts 0 or 1, case-insensitively like other SET keys. It is a
configuration setting, not a voice parameter. With AUTOLOAD 1 saved, boot
restores the snapshot. With AUTOLOAD 0 saved, boot uses factory voice/volume
and an empty dictionary; the saved snapshot remains available to LOAD_CONFIG.
After LOAD_CONFIG, its saved autoload policy is restored too.

Save only after tuning, rather than after every SET. Repeating SAVE_CONFIG
with identical settings does not erase/program flash. Flash operations pause
idle audio DMA and I2S clocks and coordinate both CPU cores while XIP flash
is unavailable. Wait for the reply before sending anything else: USB and UART
are not serviced normally during the write. Allow several seconds in a host
application; do not stream commands during saving or erasing.

The last 8 KiB of the Zero's 2 MiB flash is reserved by the linker. Two
alternating, versioned records include a CRC and commit marker. During a save,
the prior valid record is retained until the new record validates. If one is
incomplete or corrupt, the newest valid record is loaded. If neither is valid,
the firmware boots factory defaults. This does not replace a host-side backup.
An interrupted explicit ERASE_CONFIG can leave an old record recoverable;
wait for `OK erased` before treating erasure as complete.

Normal supplied UF2 updates write the application region and leave the saved
sectors alone. A full-chip erase, another application using those sectors, or
an incompatible future schema can lose or invalidate settings. This version
supports the Zero's 2 MiB flash layout only. Loading the 0.3.0 fallback ignores
saved settings; it has no flash configuration commands.

#### Hardware acceptance test

1. Stop/wait for any speech to finish.
2. Select your voice, volume, and pronunciation overrides.
3. Send SET AUTOLOAD 1, then SAVE_CONFIG; wait for OK saved/unchanged.
4. Disconnect and reconnect power. Send STATUS: verify the saved parameters,
   volume, rule count, saved=1, and autoload=1.
5. Speak a phrase using an override; confirm audio still works after a save.
6. Change a parameter without saving, then LOAD_CONFIG; confirm it is restored.
7. Optionally save AUTOLOAD 0, power-cycle, confirm factory startup, then use
   LOAD_CONFIG to recover the saved voice. Re-enable AUTOLOAD and save if desired.

To remove persistent settings and return to a factory boot, send DEFAULTS and
ERASE_CONFIG while idle, waiting for each reply. DEFAULTS alone is temporary;
DEFAULTS followed by SAVE_CONFIG deliberately saves factory settings.

## 6. Voice settings and presets

### Settings

| Parameter | Range | Meaning |
| --- | --- | --- |
| `VOLUME` | 0–400 | Master output percent; 0=mute, 100=original, above 100=boost |
| `AUTOLOAD` | 0 or 1 | Boot policy; persist with SAVE_CONFIG; not a voice setting |
| `PITCH` | 0–255 | SAM pitch period; lower generally sounds higher |
| `SPEED` | 0–255 | SAM timing parameter; higher generally sounds slower |
| `MOUTH` | 0–255 | Original SAM formant transformation |
| `THROAT` | 0–255 | Original SAM formant transformation |
| `SING` | 0 or 1 | SAM singing mode |
| `EXPRESSION` | 0–100 | Added pitch steps, capitalization emphasis, and emphasis timing |
| `METALLIC` | 0–100 | Dry/wet mix of 140 Hz square-wave ring modulation |
| `ROUGHNESS` | 0–100 | Sample quantization, from 8 down to 3 effective bits |

PITCH is not hertz and SPEED is not words per minute. The full original byte
ranges are available for experimentation, but extreme combinations can sound
unintelligible or behave unexpectedly. Begin near a preset and change one
parameter at a time.

MOUTH and THROAT change vocal resonance; they are not independent physical
measurements of a mouth or throat. Try small changes and compare the same
sentence. METALLIC and ROUGHNESS alter texture, not output volume.
ROUGHNESS is stepped internally, so adjacent values may sound identical.

SING changes the original engine's pitch behavior. For a deliberately flat
voice, also set EXPRESSION to 0: otherwise the added planner can still change
base pitch between segments. EXPRESSION 0 disables added pitch/timing changes,
but punctuation pauses and text normalization still occur in SAY mode.

### Built-in presets

| Character | Pitch | Speed | Mouth | Throat | Expression | Metallic | Roughness | Sing |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| CLASSIC | 64 | 72 | 128 | 128 | 0 | 0 | 0 | 0 |
| COMPANION | 58 | 70 | 145 | 135 | 65 | 0 | 0 | 0 |
| GUARDIAN | 70 | 78 | 135 | 155 | 40 | 10 | 5 | 0 |
| ENFORCER | 82 | 84 | 120 | 175 | 30 | 30 | 15 | 0 |
| DICTATOR | 94 | 90 | 110 | 185 | 45 | 45 | 20 | 0 |
| MALFUNCTION | 100 | 96 | 100 | 195 | 80 | 70 | 65 | 1 |

Without a valid autoloaded configuration, the startup preset is COMPANION. Presets are starting points, not reproductions
of particular toys or television voices. CLASSIC removes added expression and
effects, but still uses the new normalization layer; it is not bit-exact
original-machine emulation.

To preserve your custom setup, use SAVE_CONFIG while idle. Keep a host-side
copy of the setup commands as a portable backup. Selecting another preset overwrites
your voice adjustments, so apply the preset first, then SET commands.

## 7. Pronunciation and automatic delivery

### Custom pronunciations

Overrides match a complete token, before built-in acronym/number expansion.
They are not substring replacements or recursive dictionaries. A replacement
is sent onward as spelling for SAM's reciter; it is not processed through the
normalization dictionary again.

Examples developed while auditioning this project:

```text
PRONOUNCE operational=opperational
PRONOUNCE identify=I dentify
PRONOUNCE immediately=eemeedeiatly
SAY All systems are operational. Identify yourself immediately!
```

Treat these as tuning examples, not universal English spelling corrections.
Different voice settings can affect perceived pronunciation.

| Dictionary property | Limit |
| --- | --- |
| Custom entries | 12 |
| Term length | 1–15 ASCII characters |
| Term characters | Letters, digits, hyphen |
| Replacement length | 1–63 ASCII characters |
| Replacement characters | Letters, spaces, apostrophes |

Adding the same term again updates its existing slot. Rules survive CHARACTER changes and are included in SAVE_CONFIG. They are
restored on boot when autoload is enabled. There is no individual delete
command; DEFAULTS clears the RAM dictionary without erasing flash. To neutralize an entry without resetting, replace it with
an appropriate spelling, understanding that the slot remains occupied.
Phoneme-based dictionary entries are not supported in this version.

### Built-in word handling

| Text | Default interpretation |
| --- | --- |
| CPU, USB, AI, SPI, LED | Letter names |
| I2C | "eye squared see" |
| UART | "you art" |
| NASA, RADAR, LASER, RAM, ROM | Words |
| OK | "okay" |
| Dr., Mr., Mrs., approx. | Basic expanded words |
| RX-7 | "ar ex seven" |
| 12 | "twelve" |
| 007 | "zero zero seven" |
| 3.3 | "three point three" |
| V, Hz, mA after a numeric-looking token | volts, hertz, milliamps |

Ordinary whole numbers up to 9999 use cardinal words. Larger numbers and
leading-zero strings fall back to digit names. Unit handling is basic: it
does not distinguish "one volt" from "two volts". Dates, currency, fractions,
and general context-sensitive abbreviation interpretation are not implemented.

Unknown short uppercase words of 2–4 letters in mixed-case messages are
normally spelled out. A small list keeps common words such as WHAT and STOP
as words. Longer uppercase words can receive emphasis. An all-uppercase message
suppresses guessing and capitalization emphasis, but known built-ins and custom
rules still apply. Use an override for ambiguous names or acronyms.

### Punctuation and expression

- Commas introduce a nominal 100 ms pause.
- Colons and semicolons introduce a nominal 180 ms pause.
- Sentence endings introduce a nominal 240 ms pause.
- Ellipses introduce a nominal 450 ms pause.
- With EXPRESSION enabled, yes/no-style questions generally finish higher.
  Questions beginning who, what, where, when, why, how, which, or whose generally
  finish lower. This is a first-word rule, not grammatical understanding.
- Exclamation marks add emphasis. Repeating them does not currently add
  progressively greater intensity.
- Mixed-case messages can emphasize uppercase words; acronym expansion is a
  separate decision.

The implementation changes settings between speech segments, sometimes
separating a final word. Audible joins are possible. It does not yet generate
continuous phoneme-frame pitch envelopes or infer sarcasm and emotional meaning.

### Supported input characters

Use plain ASCII English letters and digits, spaces, normal punctuation
`. , ! ? : ;`, and internal apostrophes/hyphens such as `can't` or `RX-7`.
Double quotes and parentheses are ignored by the text planner. Unsupported
symbols such as `@`, `%`, and `$` are rejected. Write "percent" or "dollars"
as words. Use straight apostrophes and three periods, not smart quotes or a
Unicode ellipsis. Unicode/non-ASCII bytes are rejected by the serial receiver.

## 8. Direct phoneme speech

PHONEMES lets you choose pronunciation directly. Use uppercase SAM symbols,
concatenated within words and separated by spaces between words. The backend
also uppercases the supplied string. No dictionary rules are applied.

```text
PHONEMES /HEHLOW
```

Here `/H`, `EH`, `L`, and `OW` form the phoneme sequence. A standalone `H`
is not a substitute for `/H`.

### Common phoneme symbols

| Type | Symbols |
| --- | --- |
| Vowels | IY, IH, EH, AE, AA, AH, AO, OH, UH, UX, ER, AX, IX |
| Diphthongs | EY, AY, OY, AW, OW, UW |
| Voiced consonants | R, L, W, Y, M, N, NX, B, D, G, J, Z, ZH, V, DH |
| Unvoiced consonants | S, SH, F, TH, P, T, K, CH, /H |
| Other | Q (glottal stop), UL, UM, UN |

Examples: IY is the vowel in "feet", EH in "beg", AE in "Sam", AY in "high",
OW in "slow", UW in "crew", NX in "song", and DH in "then".

Stress digits **1–8** follow phonemes, for example:

```text
PHONEMES /HEH4LOW
```

Stress is part of the SAM phonetic notation, not a volume command. Audition
changes; there is no supported interpretation of arbitrary IPA input.
See the [reference phoneme documentation](https://github.com/pschatzmann/arduino-SAM/blob/main/PHONES.md)
for further notation examples.

Direct input is limited to **95 bytes**. Invalid symbols can produce
`ERR synthesis failed`. A synthesis error can occur after some audio has
already played. Text-planner punctuation pauses and added EXPRESSION rules
are bypassed; the original engine's own phonetic behavior still applies.

## 9. Recipes

Send setup commands one line at a time, waiting for each OK. After SAY,
wait for completion before sending another SAY.

### Friendly robot

```text
CHARACTER COMPANION
SAY Hello, my friend! Are you ready?
```

### Menacing voice found during project tuning

```text
CHARACTER CLASSIC
SET PITCH 93
SET SPEED 148
SET MOUTH 101
SET THROAT 105
SET SING 0
PRONOUNCE operational=opperational
PRONOUNCE identify=I dentify
PRONOUNCE immediately=eemeedeiatly
SAY Stop! Identify yourself immediately!
```

CLASSIC already disables EXPRESSION, METALLIC, and ROUGHNESS. This recipe uses
standard SAM voice settings with no added effects.

### Initial Cylon-inspired audition

```text
CHARACTER CLASSIC
SET PITCH 100
SET SPEED 90
SET MOUTH 110
SET THROAT 170
SET SING 1
SAY By your command.
```

This reproduces the settings of the [initial test WAV](../demos/cylon-sam.wav).
There is no built-in CYLON character. The menacing recipe above is a separate
candidate developed afterward.

### More metallic texture

```text
CHARACTER ENFORCER
SET METALLIC 60
SET ROUGHNESS 40
SAY You have been warned.
```

To remove added effects without changing the other voice settings:

```text
SET METALLIC 0
SET ROUGHNESS 0
```

## 10. Replies and host-controller operation

### Reply/event meanings

| Reply/event | Meaning |
| --- | --- |
| `READY ...` | Startup announcement; USB may miss it |
| `OK` | A configuration command succeeded |
| `ACCEPTED` | A speech request was handed to the worker; validation can still fail |
| `STARTED` | Planning succeeded and synthesis started; audio need not be audible yet |
| `DONE underruns=N clipped=M` | All scheduled speech blocks completed DMA playback |
| `OK stopping` | A busy request was asked to stop |
| `STOPPED underruns=N clipped=M` | The worker returned and queued/scheduled blocks were drained |
| `OK idle` | STOP was received while idle |
| `IDLE ...` / `BUSY ...` | STATUS result |
| `ERR ...` | A command, planning step, or synthesis operation failed |

I2S can retain a few samples in its peripheral/DAC pipeline after DMA finishes.
`DONE` is not confirmation of analog speaker output or a functioning amplifier.

`clipped` counts generated samples saturated by volume boost; it resets per
request. Clipping can be expected above 100 and can increase perceived loudness
at the cost of distortion. It is separate from underruns.

Underruns count silence blocks scheduled after playback began while the
producer was still active. They reset per speech request. Aim for zero and
report persistent nonzero results with the exact phrase and settings. This
counter does not detect every possible I2S FIFO or analog audio problem.

### Recommended controller sequence

1. Open the connection and send STATUS. Recover from BUSY with STOP if needed.
2. Send the desired CHARACTER, SET, and PRONOUNCE lines, waiting for each OK.
3. Send one SAY line.
4. Read ACCEPTED, then STARTED, then DONE; handle ERR at any stage.
5. Send the next utterance after DONE. After STOP, wait for STOPPED instead.

There is no exposed speech queue. A new SAY or PHONEMES during a request is
rejected with `ERR busy`. Configuration commands remain usable, but the
running request keeps the voice/dictionary snapshot taken at acceptance.
Master VOLUME is the exception: it applies to subsequently generated samples
even during the running request, subject to buffered-audio latency.
STATUS, STOP, HELP, and their replies may interleave with speech events.

Do not paste a whole performance script without pacing. A serial terminal's
line delay is not a reliable substitute for waiting for speech completion.
The current UART receiver has no hardware flow control, so send complete
bounded lines and wait for replies rather than flooding commands.

The device does not provide request IDs or a reset command. A practical host
should use timeouts appropriate to phrase length and speed, and resynchronize
using STATUS/STOP after connection loss. Very slow speech settings can make
an utterance last substantially longer than expected.

## 11. Limits and errors

### Limits

| Item | Limit |
| --- | --- |
| SAY text | 1–255 ASCII bytes after `SAY ` |
| Words per SAY | 40 |
| Individual input token | 31 bytes |
| Direct PHONEMES input | 1–95 bytes |
| Internal speech segments | 40; each at most 95 text bytes |
| Custom pronunciation slots | 12 |
| Pronunciation term/replacement | 15 / 63 bytes |
| Entire received command line | 270 bytes, excluding CR/LF |
| Concurrent speech requests | 1 |

Expansion can hit an internal bound before the original text reaches 255 bytes.
Split long material at sentence or clause boundaries and wait for completion
between requests. A response to one speech request is not a guarantee that
arbitrary later input will fit.

### Errors

| Error text | Action |
| --- | --- |
| `ERR unknown command` | Use uppercase command words and exact supported forms; remove leading spaces |
| `ERR unknown character` | Use one of the six preset names, without extra trailing spaces |
| `ERR invalid setting` | Check the parameter name, integer range, and absence of extra arguments |
| `ERR invalid pronunciation` | Check term/replacement lengths, allowed characters, and dictionary capacity |
| `ERR busy` | Wait for DONE or STOPPED before sending speech |
| `ERR command too long` | Shorten the line; also remove non-ASCII/control bytes such as backspace or escape sequences |
| `ERR text must contain 1..255 bytes` | Supply nonempty text within the limit |
| `ERR no speakable words` | Include words or numbers rather than punctuation alone |
| `ERR unsupported text symbol` | Replace unsupported symbols with ordinary words |
| `ERR too many words` / `ERR word too long` | Split the utterance or shorten the token |
| `ERR too many phrases` / `ERR too many speech segments` | Split the utterance |
| `ERR pronunciation expansion too long` / `ERR speech segment too long` | Shorten expanded content or spelling overrides |
| `ERR phoneme input must be under 96 bytes` | Shorten the phonetic request |
| `ERR ASCII text required` | Use plain ASCII; firmware reception normally rejects such bytes earlier |
| `ERR synthesis failed` | Check phoneme notation; retry a short ordinary phrase with CLASSIC |

ACCEPTED can be followed directly by an ERR without STARTED. It acknowledges
submission, not successful pronunciation or guaranteed audio output.

## 12. Troubleshooting

| Symptom | Checks |
| --- | --- |
| No USB serial port | Use a data cable; check the selected port; make sure normal firmware, not BOOT mode, is running |
| Board appears only as RPI-RP2 | Copy the chosen UF2, then let it reboot |
| No boot banner | Send STATUS; the USB terminal may have connected too late |
| No command response | Confirm line ending, uppercase command, correct UART baud, crossed TX/RX, and common ground |
| Typed text is invisible | Enable local echo or a terminal line-entry box |
| Gibberish on UART | Check 115200/8N1 and 3.3 V logic; USB baud selection does not reconfigure UART |
| ERR after editing a line | Avoid sending backspace/escape keys directly; compose a clean line before transmission |
| DONE but no sound | Check audio=I2S, firmware-version pin mapping, amplifier power/ground, speaker connection, and SD not being held low |
| Clicks or broken speech | Check supply, wiring, underruns, and whether coarse segment joins or effects explain the sound |
| Loud distortion | Reduce VOLUME if boosted, check amplifier hardware gain, speaker/load and supply, and disable added effects |
| Speech is too quiet | Check VOLUME, 5 V amplifier power, speaker impedance/sensitivity and hardware gain; try VOLUME 150 or 200 |
| Reset during speech | Check supply current and wiring; do not power the speaker amplifier from the Pico's 3.3 V regulator |
| Wrong pronunciation | Test CLASSIC and a PRONOUNCE override, or use PHONEMES |
| Settings disappeared | Use SAVE_CONFIG and SET AUTOLOAD 1 before reboot; without a valid autoloaded snapshot, boot uses factory settings |
| Voice changed unexpectedly | CHARACTER replaces all voice settings; apply SET commands after it |
| SAVE_CHARACTER is rejected | Use firmware SAVE_CONFIG/LOAD_CONFIG; desktop SAVE_CHARACTER/LOAD_CHARACTER are file-based commands |

When reporting a fault, include firmware version/backend, amplifier and speaker,
power source, the complete setup commands and phrase, STATUS output, and all
replies. Distinguish an ERR response from successful completion with no sound.

## 13. Building and checking the firmware

Prebuilt downloads require no compiler. For a custom build, use Git, CMake
3.20+, Make or Ninja, Python 3, a host C/C++ compiler, and an `arm-none-eabi`
C/C++ toolchain with newlib. The repository pins Pico SDK 2.2.0.

From the repository root:

```sh
make reference
make sdk
make firmware-pwm
make firmware-i2s
make firmware-test
```

The hardware-tested 0.3.0 fallback is kept under [downloads/v0.3.0](../downloads/v0.3.0/README.md).
New flash persistence needs its own physical power-cycle test.

The results are `build/pwm/robot-sam-PWM.uf2` and
`build/i2s/robot-sam-I2S.uf2`. Read [THIRD_PARTY.md](../THIRD_PARTY.md) for
reference-engine licensing and included notices.

In a fresh CMake build directory, PWM is the default. For I2S:

```sh
cmake -S firmware -B build/custom-i2s -DCMAKE_BUILD_TYPE=Release \
  -DAUDIO_OUTPUT=I2S -DROBOT_I2S_DATA_PIN=10 -DROBOT_I2S_CLOCK_PIN=9
cmake --build build/custom-i2s -j2
```

`ROBOT_I2S_CLOCK_PIN` specifies BCLK; **LRC is always BCLK − 1** in 0.4.0.
DIN can use another available GPIO. Active audio pins must not overlap UART.
Pin changes are compile-time, not runtime. When upgrading a previous build,
explicitly set the new clock pin or use a fresh directory: CMake preserves
cached settings even if source defaults change.

`ROBOT_PWM_PIN` defaults to 14 and must be even (PWM channel A). UART0 TX/RX
defaults to GP0/GP1; supported alternatives are checked during configuration.
Changing baud requires a source change in this version, not a CMake switch.
Refer to the [firmware development notes](../firmware/README.md) for build details.

Firmware tests validate UF2 structure/content, simulate the shipped I2S PIO
instruction timing, and reject invalid pin/backend configurations. `make test`
checks desktop speech/planning; `make sanitize` checks the planner under
AddressSanitizer/UBSan. Native configuration tests also simulate interrupted
programming, corrupted records, sequence rollover, and the real storage adapter
against mocked NOR flash. These are not substitutes for physical-board audio,
electrical, and sustained-operation testing.

## 14. Current omissions

These are not hidden commands; they are absent from the supplied firmware:

- Hardware GAIN-pin control, potentiometer input, and SD-pin control. Digital VOLUME is implemented.
- Baud selection, slow-serial protocol support, short command aliases, and BUSY pin.
- I2C/SPI command transports. **I2S audio is implemented; I2C control is not.**
- Custom named character banks and individual pronunciation deletion. A single
  flash configuration is implemented through SAVE_CONFIG/LOAD_CONFIG; desktop
  SAVE_CHARACTER/LOAD_CHARACTER remain separate file-based commands.
- Queued speech, runtime PWM/I2S switching, request IDs, and remote reset.
- Independent formant controls and continuous phoneme-level pitch envelopes.
- Speech recognition, microphone input, voice sampling, and recording playback.

The normal intended operation remains simple: initialize one character, send
text, wait for completion, and repeat.
