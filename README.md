# MidiController (C++)

Portable C++17 rewrite of a guitar-pedal MIDI controller's brain, ported from the
Raspberry Pi Python version. Hexagonal design: a hardware-free domain core with all
I/O behind interfaces, so the same code runs on the desktop sim now and a
microcontroller later.

Status: domain core + config + simulator are done and tested. Microcontroller
adapters and the editor app are not started. Roadmap: [docs/plan.md](docs/plan.md).

## Build, test, run (WSL)

Needs `g++` (C++17) and `make`. GoogleTest and nlohmann/json are vendored, so there
are no other dependencies.

```sh
make build   # -> build/midicontroller
make test    # build & run all tests (unit + mock + e2e)
make run     # run the simulator on data/ (scripted demo)
make clean
```

## Layout

```
include/mc/   headers: domain/ ports/ config/ adapters/ app/
src/          implementations + main.cpp
data/         config as JSON: midi_controller.json, pedals/, songs/, sets/
tests/        unit/ mock/ e2e/ support/
tools/        yaml2json.py (one-shot YAML->JSON converter)
third_party/  vendored googletest + nlohmann/json
docs/         plan, architecture, domain, midi-protocol, config-format
```

## Notes

- The MIDI output matches the original Python byte-for-byte, including its quirks
  (e.g. `Set Preset`'s `cc:0` bank is never sent). See [docs/midi-protocol.md](docs/midi-protocol.md).
- `data/*.json` is generated from the original YAML by `tools/yaml2json.py` and committed.

## Docs

- [docs/architecture.md](docs/architecture.md) — ports & adapters + data-flow diagrams (mermaid)
- [docs/domain.md](docs/domain.md) — the core modules
- [docs/midi-protocol.md](docs/midi-protocol.md) — byte construction & dispatch
- [docs/config-format.md](docs/config-format.md) — JSON config reference
