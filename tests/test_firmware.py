"""Check built UF2 payloads and simulate the actual PIO instruction stream.

These checks cannot validate hardware clocks, wiring, DMA execution, or sound.
Run after building both firmware variants.
"""
import pathlib
import re
import struct

ROOT = pathlib.Path(__file__).resolve().parents[1]


def check_uf2(backend):
    directory = ROOT / "build" / backend.lower()
    binary = (directory / f"robot-sam-{backend}.bin").read_bytes()
    uf2 = (directory / f"robot-sam-{backend}.uf2").read_bytes()
    assert uf2 and len(uf2) % 512 == 0
    blocks = len(uf2) // 512
    recovered = bytearray()
    for index in range(blocks):
        block = uf2[index * 512:(index + 1) * 512]
        magic1, magic2, flags, address, size, number, count, family = struct.unpack_from("<8I", block)
        assert (magic1, magic2) == (0x0A324655, 0x9E5D5157)
        assert flags & 0x2000 and family == 0xE48BFF56
        assert struct.unpack_from("<I", block, 508)[0] == 0x0AB16F30
        assert size == 256 and number == index and count == blocks
        assert address == 0x10000000 + index * 256
        recovered.extend(block[32:32 + size])
    assert recovered[:len(binary)] == binary
    assert all(x == 0 for x in recovered[len(binary):])
    assert f"Robot SAM {backend}".encode() in binary
    print(f"{backend}: {blocks} valid RP2040 UF2 blocks, {len(binary)} flash bytes")


def check_i2s():
    # Parse the instructions we ship rather than maintaining a separate program.
    instructions, labels = [], {}
    for raw in (ROOT / "firmware/i2s.pio").read_text().splitlines():
        line = raw.split(";")[0].strip()
        if not line or line.startswith("."):
            continue
        if line.endswith(":"):
            labels[line.removeprefix("public ")[:-1]] = len(instructions)
        else:
            instructions.append(line)
    word = 0x1234ABCD
    pc, x, data, clock, ws = labels["entry_point"], 0, 0, 0, 0
    bit = 0
    captured = []
    for _ in range(64 * 3 + 1):
        line = instructions[pc]
        side = re.search(r"side 0b([01]{2})", line)
        assert side, line
        pins = int(side[1], 2)
        old_clock = clock
        clock, ws = pins >> 1, pins & 1
        next_pc = (pc + 1) % len(instructions)
        if line.startswith("out pins, 1"):
            data = (word >> (31 - bit % 32)) & 1
            bit += 1
        elif line.startswith("set x, 14"):
            x = 14
        elif line.startswith("jmp x-- "):
            if x:
                next_pc = labels[line.split()[2]]
            x = (x - 1) & 0xFFFFFFFF
        else:
            raise AssertionError(line)
        if not old_clock and clock and bit:
            captured.append((data, ws))
        pc = next_pc
    assert len(captured) == 96
    for i, (data, ws) in enumerate(captured):
        assert data == (word >> (31 - i % 32)) & 1
        # WS changes one bit before the next channel's MSB (Philips I2S).
        assert ws == (0 if 15 <= i % 32 <= 30 else 1)
    print("I2S PIO: MSB-first data, 16-bit slots, one-bit WS delay, 64 cycles/frame")


check_uf2("PWM")
check_uf2("I2S")
check_i2s()
