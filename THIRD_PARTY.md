# Reference engine

The optional speech backend uses pschatzmann/arduino-SAM, pinned at
`6c3c08b69d8d98d8b09462eb7285be20f84d8a20`:
https://github.com/pschatzmann/arduino-SAM

It is downloaded by `make reference`, rather than copied into this repository.
Its README says that the reverse-engineered original SAM software has no
specific open-source license and identifies SoftVoice, Inc. as the copyright
holder. The term "abandonware" does not establish permission to redistribute.
This prototype does not resolve those rights. Review the upstream notices and
establish an appropriate basis before distributing firmware containing it.

The planner and transport-facing API are separate from this reference backend,
so an independently implemented synthesizer can replace it.
