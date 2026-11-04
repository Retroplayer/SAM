"""Exercise the actual reference synthesis backend and command-session state."""
import pathlib
import struct
import subprocess
import tempfile
import wave

ROOT = pathlib.Path(__file__).resolve().parents[1]
EXE = ROOT / "build/robot-sam"


def run(*args, **kwargs):
    return subprocess.run([str(EXE), *args], text=True, capture_output=True,
                          timeout=20, **kwargs)


def read_audio(path):
    raw = path.read_bytes()
    assert struct.unpack_from("<I", raw, 4)[0] + 8 == len(raw)
    with wave.open(str(path), "rb") as wav:
        assert (wav.getnchannels(), wav.getsampwidth(), wav.getframerate()) == (1, 1, 22050)
        data = wav.readframes(wav.getnframes())
        assert 2205 < len(data) < 22050 * 60
        assert max(data) - min(data) > 20, "silent or near-silent speech"
        assert sum(abs(x - 128) for x in data) > len(data)
        return data


with tempfile.TemporaryDirectory() as directory:
    temp = pathlib.Path(directory)
    recordings = []
    for character in ("COMPANION", "GUARDIAN", "ENFORCER", "DICTATOR", "MALFUNCTION", "CLASSIC"):
        path = temp / f"{character}.wav"
        result = run("--character", character, "--wav", str(path),
                     "Hello, my friend! Is the CPU ready?")
        assert result.returncode == 0, result.stderr
        recordings.append(read_audio(path))
    assert len(set(recordings)) == 6, "characters must produce distinct audio"

    result = run("--console", cwd=temp, input=(
        "CHARACTER DICTATOR\nPRONOUNCE ROS=ross\nSET METALLIC 55\n"
        "SAY ROS controls the CPU!\nSAY RX-7 has 12 V.\nSAVE_CHARACTER robot.profile\nSTATUS\nQUIT\n"))
    assert result.returncode == 0, result.stderr
    assert "metallic=55" in result.stdout and "rules=1" in result.stdout
    read_audio(temp / "speech-001.wav")
    read_audio(temp / "speech-002.wav")
    result = run("--console", cwd=temp, input="LOAD_CHARACTER robot.profile\nSTATUS\nQUIT\n")
    assert result.returncode == 0, result.stderr
    assert "pitch=94" in result.stdout and "metallic=55" in result.stdout and "rules=1" in result.stdout
    (temp / "bad.profile").write_text("ROBOT-SAM 1\nVOICE 0 0 0 0 101 0 0 0\nEND\n")
    result = run("--console", cwd=temp, input="LOAD_CHARACTER bad.profile\nSTATUS\nQUIT\n")
    assert result.returncode != 0 and "pitch=58" in result.stdout

    path = temp / "phonemes.wav"
    result = run("--phonemes", "--wav", str(path), "/HEHLOW")
    assert result.returncode == 0, result.stderr
    read_audio(path)

    for text in ("", "...", "x" * 256, "a" * 32, "voltage @bad"):
        path = temp / "invalid.wav"
        result = run("--wav", str(path), text)
        assert result.returncode != 0
        assert not path.exists(), "invalid text must not create an audio file"
    result = run("--console", cwd=temp, input="SET PITCH -1\nSET SPEED 30garbage\n")
    assert result.returncode != 0

    # Volume is an output setting: character selection must preserve it.
    levels = {}
    for gain in (0, 50, 100, 200, 400):
        folder = temp / f"volume-{gain}"
        folder.mkdir()
        result = run("--console", cwd=folder, input=(
            f"SET VoLuMe {gain}\nCHARACTER CLASSIC\nSTATUS\n"
            "SAY All systems are operational.\nQUIT\n"))
        assert result.returncode == 0, result.stderr
        assert f"volume={gain}" in result.stdout
        with wave.open(str(folder / "speech-001.wav"), "rb") as wav:
            levels[gain] = wav.readframes(wav.getnframes())
        assert "clipped=0" in result.stdout if gain <= 100 else "clipped=0" not in result.stdout
    assert len({len(data) for data in levels.values()}) == 1
    assert set(levels[0]) == {128}, "mute must produce digital silence"
    baseline = levels[100]
    for gain in (50, 200, 400):
        # Same synthesis in independent processes; verify the actual gain path.
        for before, after in zip(baseline, levels[gain]):
            value = int((before - 128) * 256 * gain / 100)
            expected = (max(-32768, min(32767, value)) + 32768) // 256
            assert after == expected
    rms = lambda data: (sum((x - 128) ** 2 for x in data) / len(data)) ** 0.5
    assert rms(levels[50]) < rms(baseline) < rms(levels[200]) < rms(levels[400])
    for setting in ("401", "-1", "200junk", "999999999999999999999"):
        result = run("--console", input=f"SET VOLUME {setting}\nSTATUS\nQUIT\n")
        assert result.returncode != 0 and "volume=100" in result.stdout

print("Audio checks passed: six voices, session state, phonemes, WAV integrity, volume/mute/boost, and rejected input.")
