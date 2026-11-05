# Robot SAM 0.5.0 user manual

**RP2040-Zero: USB editor, compact robot UART, and USB UART simulation.**

[Download I2S firmware](https://github.com/Retroplayer/SAM/raw/refs/heads/main/downloads/robot-sam-I2S.uf2) ·
[Download PWM firmware](https://github.com/Retroplayer/SAM/raw/refs/heads/main/downloads/robot-sam-PWM.uf2) ·
[0.4.0 fallback and manual](../downloads/v0.4.0/README.md)

**Protocol change:** UART is now a dedicated speech interface. It accepts short
commands and returns only `A`, `D`, `R`, `B`, or `E`, without line endings.
All editing, help, diagnostics, and flash operations are USB-only. Nothing is
mirrored between ports. Existing UART applications using long 0.4.0 commands
must be updated, or continue using the fallback firmware.

## 1. Connect and flash

Hold BOOT while connecting USB (or hold BOOT while pressing/releasing RESET),
then copy the desired UF2 onto `RPI-RP2`. The board restarts. Open its USB CDC
serial port in Arduino Serial Monitor or another Windows serial terminal.
Use a data-capable USB cable. No special PC application is needed.

**Use a terminal with a text-entry box or local line editing.** Compose/correct
the entire editor command before sending it. Set LF or CRLF as the line ending.
The firmware does not implement interactive cursor movement, backspace editing,
echo, ANSI menus, or pagination. It buffers the line and executes at CR/LF.
Use plain 7-bit ASCII. Command verbs are uppercase; voice parameter keys and
built-in preset names are case insensitive. Help topics are uppercase.

The USB baud selector does not change physical UART settings. USB's 1200-baud
bootloader-reset shortcut is disabled so changing a terminal's baud setting
does not unintentionally reboot this firmware. Use BOOT to reflash.

Send:

```text
HELP
STATUS
VOICE LIST
SAY Hello, my friend!
```

There is no UART boot banner and no unsolicited USB banner to miss. STATUS
reports version, audio backend, owner, saved/migrated/dirty flags, selected
editor voice, startup voice, live robot volume, workspace volume, dictionary
count, current UART framing/baud, and last completion counters.

### Wiring

| Connection | GPIO / destination |
| --- | --- |
| MAX98357A LRC / WS | GP8 |
| MAX98357A BCLK | GP9 |
| MAX98357A DIN | GP10 |
| PWM output (PWM build only) | GP14, through filter into analog amplifier |
| Robot host TX → Pico RX | GP1 |
| Pico TX → robot host RX | GP0 |
| Ground | Common between Pico, host, and amplifier |

The pictured MAX98357A module's header order is LRC, BCLK, DIN, GAIN, SD,
GND, VIN. Leave GAIN and SD at module defaults initially. The board is marked
2.5–5.5 V input, 9 dB default gain, and averaged left/right output.

For USB-powered Waveshare RP2040-Zero, its 5V pin is USB VBUS. A sufficiently
capable USB supply can power the amplifier there. Do not power the speaker
amplifier from the Pico's 3.3 V regulator. Do not parallel external 5 V and
USB VBUS without suitable isolation. Verify clone board power arrangements.

Connect a suitable 4–8 ohm speaker across the amplifier's two speaker outputs;
neither output is ground. The advertised 3 W generally requires a 4 ohm load
at 5 V under specified distortion conditions. An 8 ohm speaker is valid but
usually allows nearer 1–2 W maximum. A speaker's wattage is power handling,
not a guarantee of acoustic loudness.

The I2S build sends duplicated mono, signed 16-bit samples in two 16-bit slots,
nominally 22,050 frames/s. No MCLK is supplied. PWM is not an analog input for
the MAX98357A: use a separate analog amplifier with filtering/AC coupling.
A 2.2 kΩ/10 nF first-order filter is one starting point for GP14; amplifier
input requirements determine additional filtering, coupling, and attenuation.
Never connect a speaker directly to GP14.

## 2. The three operating paths

| Path | Commands and replies | Configuration used |
| --- | --- | --- |
| USB editor | Readable full-line commands and CRLF responses | Editable workspace; direct speech auditions it |
| Physical UART | Short line commands, bare single-byte replies | Applied character bank and dictionary; temporary live tweaks |
| USB UART simulation | Exact same byte-fed parser and responses as UART | Applied bank/dictionary with an isolated copy of UART live tweaks |

Both physical UART and USB simulation parse incoming bytes incrementally.
**Every command, including `?` and `!`, must end with CR or LF.** CRLF also
works; the second delimiter is ignored. The compact protocol does not require
terminal echo or character editing. The USB editor expects the host terminal
to perform any editing before sending the line.

There is one speech engine. A second request is rejected while busy. Voice
and dictionary data are copied when a request starts, so later editing cannot
change an utterance halfway through. Volume can affect newly generated audio;
already prepared buffers can retain their previous level for roughly 0.1 s.

Merely connecting USB does not change UART settings or interrupt robot speech.
Disconnecting USB discards a partial USB command, returns USB to editor mode,
and cancels an audition owned by USB. It does not stop UART-owned speech.
Unsent USB output is discarded on disconnect.

## 3. Editing, applying, and saving

The **workspace** is a RAM copy of the applied configuration. Editing is safe
while the robot is speaking. It does not modify the live robot or flash.
USB SAY/PHONEMES audition this workspace immediately.

```text
VOICE SELECT 1
SET PITCH 60
SET VOLUME 118
DICT SET operational=opperational
SAY All systems are operational.
```

Wait for DONE, then:

```text
APPLY
SAVE_CONFIG
```

- **APPLY** validates the workspace, copies it to the active bank/dictionary,
  and resets live robot tweaks to the configured startup character and volume.
  It requires idle. It does not change physical UART framing/baud.
- **SAVE_CONFIG** persists the applied configuration while idle. It refuses
  dirty workspace edits: APPLY first. It does not capture temporary UART or
  simulation tweaks. Identical saves return `OK unchanged` without writing.
- **DISCARD** replaces the workspace with the applied configuration. It does
  not undo temporary UART tweaks or alter flash.
- **LOAD_CONFIG** restores the saved workspace and applied configuration while
  idle. It resets the editor selection and live voice/volume, but serial
  settings change physically only on UART APPLY or reboot.
- **DEFAULTS** puts factory settings in the workspace only. APPLY activates
  them; SAVE_CONFIG persists them. It does not erase saved data.
- **ERASE_CONFIG** erases new and legacy saved records while idle. It leaves
  current RAM configuration unchanged. To return fully to a factory boot,
  use DEFAULTS, APPLY, ERASE_CONFIG, then reboot.

Wait for each response, especially after flash operations. Do not paste or
stream more commands while a save/erase is in progress. USB and UART service
pause during flash writes. Allow several seconds in a host program.

`dirty=1` means workspace edits have not been applied. `saved=1` means a valid
stored configuration exists; it is not a promise that RAM matches that save.
`migrated=1` means settings came from a 0.4.0 record and have not yet been saved
in the new bank format.

### Startup settings

```text
SET STARTUP 1
SET VOLUME 118
SET AUTOLOAD 1
APPLY
SAVE_CONFIG
```

STARTUP must refer to an enabled character. AUTOLOAD 1 restores the saved
character bank, dictionary, startup choice, and master volume at boot.
AUTOLOAD 0 boots factory voice/dictionary/volume instead, keeping the saved
bank available to LOAD_CONFIG. **Saved UART baud/framing are restored in
both cases**, so disabling voice autoload does not unexpectedly change the
robot's serial connection.

## 4. Character editor

Eight numeric IDs, **0–7**, are available. Names are 1–23 ASCII letters,
digits, underscores, or hyphens. IDs are what the robot sends; names are for
users. Names need not be unique, so use numeric selection when ambiguous.

Factory slots:

| ID | Name | Pitch | Speed | Mouth | Throat | Expression | Metallic | Roughness | Sing |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 0 | CLASSIC | 64 | 72 | 128 | 128 | 0 | 0 | 0 | 0 |
| 1 | COMPANION | 58 | 70 | 145 | 135 | 65 | 0 | 0 | 0 |
| 2 | GUARDIAN | 70 | 78 | 135 | 155 | 40 | 10 | 5 | 0 |
| 3 | ENFORCER | 82 | 84 | 120 | 175 | 30 | 30 | 15 | 0 |
| 4 | DICTATOR | 94 | 90 | 110 | 185 | 45 | 45 | 20 | 0 |
| 5 | MALFUNCTION | 100 | 96 | 100 | 195 | 80 | 70 | 65 | 1 |
| 6–7 | Empty | | | | | | | | |

Factory startup is ID 1. A migrated 0.4.0 configuration puts the previously
saved voice at ID 1 under the name ROBOT; its parameters are preserved.

| USB command | Meaning |
| --- | --- |
| `VOICE LIST` | List enabled workspace slots; ends with OK end |
| `VOICE SHOW 6` | Show a workspace character's name and all parameters |
| `VOICE CREATE 6 ENEMY` | Create an empty slot with CLASSIC parameters |
| `VOICE RENAME 6 CYLON` | Rename an existing slot |
| `VOICE PRESET 6 ENFORCER` | Put built-in preset parameters into a slot; keep its existing name |
| `VOICE SET 6 PITCH 93` | Change one parameter in a specified slot |
| `VOICE SELECT 6` | Select workspace slot for USB SET/SAY/PHONEMES |
| `CHARACTER CLASSIC` | Select first workspace character with this name, case-insensitively |
| `VOICE DELETE 6` | Delete an existing slot; cannot delete the configured startup slot |

To replace a deleted startup character, change SET STARTUP to another valid
slot first. Selection changes alone do not mark the workspace dirty. Applying
sets the robot to the configured STARTUP, not necessarily the editor selection.

### Voice parameters

`SET key value` changes the selected workspace character. `VOICE SET id key
value` changes a particular character. Values are nonnegative decimal integers.

| Parameter | Range | Effect |
| --- | --- | --- |
| PITCH | 0–255 | SAM pitch period; smaller generally sounds higher |
| SPEED | 0–255 | Timing parameter; larger generally speaks more slowly |
| MOUTH, THROAT | 0–255 | Vocal resonance/formant transformations |
| SING | 0 or 1 | Original SAM singing mode |
| EXPRESSION | 0–100 | Added stepped inflection and emphasis timing |
| METALLIC | 0–100 | Mix of 140 Hz square-wave ring modulation |
| ROUGHNESS | 0–100 | Stepped quantization from 8 down to 3 effective bits |

PITCH is not hertz and SPEED is not words per minute. Extreme settings are
experimental. Use the same comparison phrase and change one parameter at a
time. SING 1 plus EXPRESSION 0 provides a flatter delivery. CLASSIC disables
added expression/effects; text normalization and punctuation pauses still occur.

### Master volume

SET VOLUME 0..400 edits workspace master volume. It is not part of a character.
USB auditions use this level; if USB owns the current audition, changing it
also affects subsequent samples in that audition. Applying it sets the live
robot volume. UART V changes only the temporary live volume, not the save.

- 0: digital mute.
- 50: half the original sample amplitude.
- 100: original unity output.
- 118: factory default, based on the tested robot voice.
- 200: double amplitude, approximately +6 dB before clipping.
- 400: maximum 4× boost, approximately +12 dB before clipping.

SAM already reaches full-scale peaks. Boost increases average loudness by
clipping some peaks. Saturation prevents wrapping; reduce gain if distortion
is excessive. Completion reports and STATUS include clipped sample counts.
No GPIO or amplifier GAIN connection is needed for software volume.
118 is a user-tested setting, not a guarantee of zero clipping for every voice
and phrase. Existing saved volume values are preserved when migrating.

The MAX98357A's separate GAIN pin is discrete hardware gain, not an analog pot
input. Default is 9 dB. Its standard choices are 15 dB with 100 kΩ to ground,
12 dB directly to ground, 9 dB floating, 6 dB directly to VIN, and 3 dB with
100 kΩ to VIN. Account for existing module resistors before changing these.
SD and any volume potentiometer input are not controlled by this firmware.
Do not connect a 5 V pulled-up SD node directly to a Pico GPIO without
appropriate interfacing.

## 5. Pronunciation dictionary editor

The bank supports **64 shared whole-token corrections**, applied before
built-in acronym, number, and SAM pronunciation rules. All characters use
the same applied dictionary. Edits affect USB auditioning immediately but
reach the robot only after APPLY; flash changes only after SAVE_CONFIG.

```text
DICT SET operational=opperational
DICT SET identify=I dentify
DICT SET immediately=eemeedeiatly
DICT SHOW operational
DICT LIST
```

| Command | Meaning |
| --- | --- |
| `DICT SET term=spoken words` | Add or update an entry; no spaces around = |
| `PRONOUNCE term=spoken words` | Alias for DICT SET |
| `DICT SHOW term` | Inspect one case-insensitive term |
| `DICT LIST` | List workspace entries; ends with OK end |
| `DICT DELETE term` | Delete one entry and free its slot |
| `DICT CLEAR` | Clear all workspace entries |

Terms are 1–15 ASCII letters, digits, or hyphens. Replacements are 1–63
letters, spaces, or straight apostrophes and must contain a letter. A
replacement can split a word: `identify=I dentify`. Entries are not substring
matches, recursive expansions, phoneme dictionaries, or context-sensitive rules.

Known built-ins include CPU, USB, AI, I2C, SPI, LED, UART, NASA, radar, laser,
RAM, ROM, OK, and basic title/abbreviation expansions. Unknown short uppercase
words in mixed-case messages can be spelled as letters. Common words such as
WHAT and STOP remain words. All-uppercase messages suppress automatic guessing
and uppercase emphasis, while known and custom overrides still apply.

Numbers through 9999 use cardinal words; leading-zero and larger strings fall
back to digits. Decimal fractions use digit names. V, Hz, and mA after numeric
input have basic unit expansion. Dates, currency, and general abbreviation
context remain limited. Write unsupported symbols such as percent signs as
words. Plain apostrophes, internal hyphens, and `. , ! ? : ;` are supported.
Avoid Unicode smart punctuation. Text symbols such as @, $, /, or ~ may cause
a synthesis/planning error in SAY even though the serial command itself was
accepted. Slash notation belongs in PHONEMES instead.

## 6. USB speech testing

```text
VOICE SELECT 6
SAY Stop! Identify yourself immediately!
```

Normal responses:

```text
ACCEPTED
STARTED
DONE underruns=0 clipped=0
```

USB PHONEMES bypasses the dictionary and text-normalization/delivery layer:

```text
PHONEMES /HEHLOW
PHONEMES /HEH4LOW
```

Use SAM symbols, not IPA. `/H` is the H sound; stress digits 1–8 follow
phonemes. Common vowels are IY, IH, EH, AE, AA, AH, AO, OH, UH, UX, ER, AX,
IX; diphthongs EY, AY, OY, AW, OW, UW; consonants include R, L, W, Y, M, N,
NX, B, D, G, J, Z, ZH, V, DH, S, SH, F, TH, P, T, K, CH, /H.
[Reference phoneme notation](https://github.com/pschatzmann/arduino-SAM/blob/main/PHONES.md).

STOP from the USB editor can stop an audition or explicitly stop robot speech.
It replies OK stopping; the terminal completion event goes to the connection
that owns the request. If UART owned it, the robot receives R; USB does not
receive a duplicate completion. STOP while idle replies OK idle.

STARTED means synthesis started, not guaranteed sound at the speaker. A
planning failure can follow ACCEPTED without STARTED. A rendering failure can
occur after partial audio. DONE means scheduled DMA playback completed; it
does not confirm amplifier power or analog speaker output.

Underruns count silence blocks scheduled while the producer was still active.
Clipped counts samples limited by volume boost, including generated samples
later discarded by a stop. Neither is a comprehensive hardware fault detector.
Aim for zero underruns; clipping above volume 100 can be expected.

## 7. Robot UART protocol

Use 3.3 V logic, common ground, crossed TX/RX, and no hardware flow control.
True RS-232 and 5 V TX signals require appropriate level conversion.
All commands and responses are 7-bit ASCII; high-bit/control bytes invalidate
the current line. UART receive framing/parity/overrun errors also invalidate
that line, producing E at the delimiter.

Commands are case-sensitive, one per CR/LF-terminated line. **Do not send
verbose USB commands here.** There are no UART editor mode switches.

| Command | Action |
| --- | --- |
| `S text` | Speak ordinary text |
| `K phonemes` | Speak SAM phonetic notation |
| `C 6` | Select applied character ID 6 |
| `V 118` | Temporary master volume 0–400 |
| `P 93` | Temporary pitch 0–255 |
| `T 148` | Temporary speed/timing 0–255 |
| `M 101` | Temporary mouth 0–255 |
| `H 105` | Temporary throat 0–255 |
| `N 0` | Temporary singing mode 0 or 1 |
| `X 0` | Temporary expression 0–100 |
| `Y 0` | Temporary metallic effect 0–100 |
| `Z 0` | Temporary roughness 0–100 |
| `!` | Stop UART-owned speech |
| `?` | Query engine state |

C selects a character without changing volume. Temporary tweaks are never
implicitly saved and do not modify character records. C replaces previous
voice tweaks; APPLY or reboot resets them to configured startup values.
Changes during your own utterance affect the next utterance except V, which
can affect newly generated samples in the current one.

### Responses: literal bytes, no CR/LF

| Byte | Meaning |
| --- | --- |
| A | Command accepted; configuration tweak completed or speech/stop submitted |
| D | Speech finished successfully; ready for another request |
| R | Ready when queried, or requested stop completed |
| B | Busy, returned only to a state query |
| E | Rejected command or speech failure |

Typical exchanges (arrows and labels are explanatory, not transmitted):

```text
host -> S Hello!\n
SAM  -> A
SAM  -> D
host -> ?\n
SAM  -> R
host -> V 118\n
SAM  -> A
```

Wait for A after a tweak, and D after speech. A second speech request while
busy gets E. There are no request IDs or exposed speech queue. A followed by
E means submission succeeded but planning/rendering failed. If a tweak fails
while an earlier utterance is running, that utterance can still finish with D.
Use a state query to recover; inspect ERROR over USB for the detailed last error.

While busy, `!` from the owning UART returns A, then R once stopped. An idle
stop returns only R. STOP is not an amplifier shutdown and already buffered
audio can take roughly two blocks (~23 ms), plus hardware/compute latency, to
finish. Actual timing needs physical measurement. Wait for R before replacing
speech. There is no unsolicited ready byte at startup: query `?` when needed.

If USB owns the engine, UART queries receive B and UART speech/tweak/stop
commands receive E. The robot cannot stop a USB audition. USB editor STOP is
an explicit operator override; it can stop any owner. USB diagnostics and
completion messages are never mirrored onto UART.

### Physical serial settings (configured through USB only)

```text
UART SHOW
UART BAUD 2400
UART FRAME 7E1
APPLY
UART APPLY
SAVE_CONFIG
```

BAUD accepts 1200, 2400, 4800, 9600, 19200, 38400, 57600, or 115200. FRAME
accepts 8N1 or 7E1. Factory default is **9600 8N1**. Migration from a valid
0.4.0 snapshot uses **115200 8N1** to preserve its old UART setting.

UART SHOW distinguishes live settings from workspace settings. APPLY stores
new settings in the active configuration but does not alter the running
connection. UART APPLY requires idle and an applied workspace, changes the
hardware, and discards partial/pending UART input. Only USB gets its readable
acknowledgement. SAVE_CONFIG makes the active serial settings survive reboot;
boot applies them even with voice autoload disabled. USB remains a recovery
path regardless of a mistaken UART baud/framing selection.

## 8. USB UART simulation

Enter from the idle USB editor using an exact complete line:

```text
!~
```

The reply is the bare byte R. Then test the exact UART commands over USB:

```text
C 6
V 118
S Stop! Identify yourself!
?
```

Expect A for selection and volume, A then D for speech, and R/B for a query.
No STARTED messages, help text, or verbose diagnostics are inserted. ERROR,
HELP, editor commands, and flash operations are not available in simulation.

Simulation clones the robot's live voice/volume when entered. Its tweaks are
isolated and discarded on re-entry; they do not change UART live settings,
the editor workspace, or saved data. It uses the applied dictionary and bank,
so APPLY workspace changes before testing them in simulation.

To return while idle, send:

```text
~!
```

The reply is `OK editor` with CRLF; full USB commands work again. Repeating
entry/exit commands in their respective modes is harmless. A mode change
while busy is rejected; stop your simulation request or wait for idle.
Mode switches must match the complete line. They are not scanned inside
`S text`. Physical UART never recognizes them as mode changes.

This checks byte framing, command handling, ownership, and host parsing. It
**cannot test real UART baud, parity, voltage, noise, or FIFO behavior**.

## 9. Help and diagnostics

```text
HELP
HELP VOICE
HELP DICTIONARY
HELP UART
HELP TEST
HELP SAVE
STATUS
ERROR
```

HELP is a short topic list. Topics show syntax, ranges, and examples without
ANSI formatting or pagination. Unknown editor commands suggest HELP. ERROR
shows the last detailed error, including an E returned on UART/simulation.
It is a last-error register, not a per-command log; later errors replace it.

STATUS's `volume` is live robot volume; `edit_volume` is the audition/workspace
level. `rules`, `edit_voice`, `startup`, and `autoload` describe the workspace.
`UART` is the actual live serial setup; UART SHOW additionally prints the
workspace serial setup. `audio_owner` identifies UART, USB, USB_SIM, or none.
Counters describe the last completed request, not necessarily the current one.

## 10. Limits and recovery

| Item | Limit |
| --- | --- |
| Character IDs | 0–7; enabled startup character required |
| Character name | 1–23 ASCII letters/digits/underscore/hyphen |
| Shared pronunciation entries | 64 |
| Dictionary term / replacement | 15 / 63 bytes |
| Text per speech request | 1–255 ASCII bytes |
| Words per request / token length | 40 / 31 bytes |
| Direct phoneme request | 1–95 bytes |
| Internal segments | 40, each up to 95 bytes |
| Received line | 271 bytes before delimiter; long/invalid lines discarded at CR/LF |
| Simultaneous speech | One owner |

Split long text at clauses and wait for completion. Pronunciation expansion
can hit a bound before the original input reaches its maximum size. All
transports reject non-ASCII rather than silently removing the high bit.

To resynchronize after a partial or invalid UART command, send a newline,
then `?` plus newline. The discarded partial line can yield E; the subsequent
query yields R/B. Software and hardware receive errors are not checksums:
noise that changes one valid ASCII command into another can still go
undetected. No binary payloads or checksum protocol are implemented.

USB TX is buffered and drained without blocking UART polling. A 16 KiB output
queue holds a full dictionary listing. Wait for a command response instead
of flooding HELP/LIST requests. Overflow produces an error when possible;
reconnect and resend if output was lost. Physical UART still has no RTS/CTS;
use request/response pacing.

| Symptom | Check |
| --- | --- |
| Nothing when opening USB | Send HELP/STATUS; no automatic banner is required |
| USB text not visible while typing | Enable local echo or use terminal text-entry box |
| USB commands yield E only | You are in simulation; while idle send ~! as a complete line |
| UART old commands fail | Use compact S/C/V/etc.; editor is USB-only |
| UART gibberish or no response | Check UART SHOW via USB, framing/baud, TX/RX crossing, 3.3 V, common ground and newline |
| SAVE_CONFIG rejected | Wait for idle; APPLY workspace edits first |
| New character not available on UART | APPLY, then use C with its numeric ID |
| Selecting character changes nothing in robot | VOICE SELECT is USB audition selection; set STARTUP/APPLY or use UART C |
| Settings vanish after reboot | SAVE_CONFIG after APPLY; verify AUTOLOAD and saved status |
| Serial baud changes unexpectedly at boot | Check saved UART configuration; it is restored independently of voice autoload |
| DONE but no sound | Check STATUS audio backend, GP8/9/10 wiring, amplifier power, speaker, and SD state |
| Speech too quiet | Check 5 V supply, load/sensitivity, hardware gain, and volume; try 150–200 |
| Loud distortion | Reduce volume boost or hardware gain; disable effects for comparison |
| Reset during speech | Check power/cable/current capacity; do not use the 3.3 V regulator for speaker power |
| Underruns | Record exact text/voice/volume; compare without effects and report counters |
| Wrong pronunciation | Add a workspace correction and audition, or use PHONEMES |

## 11. Migration, persistence, and fallbacks

Saved 0.4.0 settings are read without erasing their legacy sectors. Voice,
volume, pronunciation corrections, and autoload policy migrate into the bank;
the old voice becomes ROBOT in slot 1, startup 1, UART 115200 8N1. On first
boot STATUS shows migrated=1. Inspect, APPLY edits if necessary, then
SAVE_CONFIG to write the new schema. Migration itself never writes flash.

New bank storage reserves two alternating 8 KiB slots; each holds a versioned
6 KiB record with sequence, CRC, and commit marker. The final 8 KiB retains
legacy records, for **24 KiB total reserved flash**. Application linker/UF2
checks exclude this region. Save preserves the previous valid bank until
verification; if neither new slot validates, a valid legacy snapshot can
still be migrated. Keep a host-side list of setup commands as a backup.

ERASE_CONFIG explicitly clears all 24 KiB so legacy data cannot reappear.
An interrupted erase may leave recoverable data; wait for acknowledgement.
Saving pauses idle DMA/PIO and safely parks the other core while flash is
unavailable. Normal voice tweaks never write flash. Do not save repeatedly
without a reason; unchanged bank saves skip programming.

A normal supplied UF2 update does not overwrite the reserved region. A full
flash erase or unrelated firmware can destroy it. Rolling back to 0.4.0 reads
its old legacy snapshot, **not later 0.5.0 character-bank edits**. Reinstalling
0.5.0 can recover its bank if the sectors were left intact.

Fallbacks: [0.4.0](../downloads/v0.4.0/README.md) and
[hardware-tested 0.3.0](../downloads/v0.3.0/README.md).
The [archived 0.4.0 manual](FIRMWARE_MANUAL_0.4.md) documents the older protocol.
New console routing, physical slow-UART framing, and flash migration need
board validation even though native protocol/storage tests and both
cross-builds pass.

## 12. Complete configuration example

Send each line separately from the USB terminal. Wait for DONE after SAY.
The example assumes ID 6 is empty; use VOICE LIST and choose an empty slot.

```text
VOICE CREATE 6 ENEMY
VOICE SET 6 PITCH 93
VOICE SET 6 SPEED 148
VOICE SET 6 MOUTH 101
VOICE SET 6 THROAT 105
VOICE SET 6 SING 0
VOICE SELECT 6
SET VOLUME 118
SET STARTUP 6
DICT SET operational=opperational
DICT SET identify=I dentify
DICT SET immediately=eemeedeiatly
SAY Stop! Identify yourself immediately!
```

After the audition completes:

```text
UART BAUD 2400
UART FRAME 7E1
APPLY
UART APPLY
SAVE_CONFIG
```

Enter USB simulation with `!~`, then try `S All systems are operational.`
Await A then D, exit with `~!`, and inspect STATUS. Power-cycle and verify
startup 6, volume 118, three rules, and saved UART 2400/7E1. Finally test the
physical UART with the actual robot or a correctly configured adapter.

## 13. Building and verification

See [firmware development notes](../firmware/README.md) for prerequisites.
From the repository root:

```sh
make reference
make sdk
make test
make firmware-pwm
make firmware-i2s
make firmware-test
```

The native tests execute the same portable editor/compact parser as firmware,
exercise byte fragmentation, response isolation, ownership, workspace/apply
rules, malformed ASCII, character/dictionary editing, and help. Storage tests
cover migration, corruption, interrupted programming, sequence rollover,
no-op writes, audio guards, and erasure. UF2 checks verify binaries and reserved
flash; PIO instructions are simulated for timing. These checks do not replace
physical power-cycle, USB terminal, UART framing, and audio testing.

The desktop `build/robot-sam` tool remains a separate WAV audition utility with
its earlier console/file-profile interface. It is not an emulator of the new
USB editor. Story sessions, temporary story banks, I2C/SPI host transports,
SD control, pot input, recording, and speech recognition remain outside this
release.
