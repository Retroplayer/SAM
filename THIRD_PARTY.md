# Reference engine

The optional speech backend uses pschatzmann/arduino-SAM, pinned at
`6c3c08b69d8d98d8b09462eb7285be20f84d8a20`:
https://github.com/pschatzmann/arduino-SAM

Its source is downloaded by `make reference`, rather than copied into this repository.
The prebuilt firmware UF2 files in `downloads/` contain its compiled code.
Its README says that the reverse-engineered original SAM software has no
specific open-source license and identifies SoftVoice, Inc. as the copyright
holder. The term "abandonware" does not establish permission to redistribute.
This prototype does not resolve those rights. Review the upstream notices and
establish an appropriate basis before distributing firmware containing it.

The planner and transport-facing API are separate from this reference backend,
so an independently implemented synthesizer can replace it.

The I2S PIO program is adapted from Raspberry Pi's pico-extras
`audio_i2s.pio`, copyright 2020 Raspberry Pi (Trading) Ltd., under BSD-3-Clause.
Its full license is included in `firmware/LICENSE.pio`.
Pico SDK and its pinned TinyUSB submodule are fetched for firmware builds;
their applicable notices are retained in those checkouts and
`downloads/SDK-LICENSE.txt`.
