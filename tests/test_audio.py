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

print("Audio checks passed: six voices, session state, phonemes, WAV integrity, and rejected input.")
