# Voice auditions

[Download the Cylon-inspired SAM test](https://github.com/Retroplayer/SAM/raw/refs/heads/main/demos/cylon-sam.wav).

This is an initial parameter candidate, not a verified match to the television
voice. It uses only standard SAM controls: pitch 100, speed 90, mouth 110,
throat 170, and singing mode on. Added expression, ring modulation, and
quantization effects are all disabled through the CLASSIC base character.

Text: "By your command. All systems are operational. You cannot escape.
The fleet will be destroyed."

To reproduce from the repository root after building:

```sh
build/robot-sam --console < demos/cylon-sam.commands
```

This writes `speech-001.wav` in the current directory. The same SET commands
can be used with the current firmware; CYLON is not yet a built-in preset.
