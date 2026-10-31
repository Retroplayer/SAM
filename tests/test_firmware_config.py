"""Ensure configuration rejects modes and pin mappings the driver cannot use."""
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
cases = [
    ("mode", ["-DAUDIO_OUTPUT=DAC"], "AUDIO_OUTPUT must be PWM or I2S"),
    ("pwm-pin", ["-DROBOT_PWM_PIN=15"], "must be even"),
    ("i2s-overlap", ["-DAUDIO_OUTPUT=I2S", "-DROBOT_I2S_DATA_PIN=8"], "must not overlap"),
    ("uart-pin", ["-DROBOT_UART_TX_PIN=2"], "Pins must select UART0"),
    ("i2s-range", ["-DAUDIO_OUTPUT=I2S", "-DROBOT_I2S_CLOCK_PIN=0"], "BCLK must be at least 1"),
]
for name, options, expected in cases:
    result = subprocess.run([
        "cmake", "-S", str(ROOT / "firmware"),
        "-B", str(ROOT / "build" / f"invalid-{name}"),
        *options,
    ], text=True, capture_output=True, timeout=60)
    assert result.returncode != 0, name
    assert expected in result.stdout + result.stderr, result.stdout + result.stderr
print("Firmware configuration: five invalid backend/pin assignments rejected.")
