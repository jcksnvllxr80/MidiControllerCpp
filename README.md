# MidiController (C++)

Portable C++17 rewrite of a guitar-pedal MIDI controller's brain, ported from the
Raspberry Pi Python version. Hexagonal design: a hardware-free domain core with all
I/O behind interfaces, so the same code runs on the desktop sim now and a
microcontroller later.

Status: domain core + config + simulator are done and tested; the microcontroller
firmware (Pico 2 W / RP2350) builds and is under hardware bring-up. (The editor app
is a separate project.)
Roadmap: [docs/plan.md](docs/plan.md).

## Build, test, run (WSL)

The **firmware** builds with CMake + the Pico SDK; the **simulator** and the **tests**
build with `g++` (C++17) — GoogleTest and nlohmann/json are vendored, so the host
build has no other dependencies.

```sh
make build       # firmware  -> build-pico/midicontroller_pico.uf2  (Pico 2 W; needs PICO_SDK_PATH)
make build-sim   # simulator  -> build/midicontroller
make test        # build & run all tests (unit + mock + e2e)
make run         # run the simulator on data/ (scripted demo)
make clean       # remove desktop build/ ;  make pico-clean removes build-pico/
```

Flashing and wiring: [docs/mcu.md](docs/mcu.md) and [docs/wiring.md](docs/wiring.md).

## Layout

```
include/mc/   headers: domain/ ports/ config/ adapters/ app/
src/          implementations + main.cpp
data/         config as JSON: midi_controller.json, pedals/, songs/, sets/
tests/        unit/ mock/ e2e/ support/
tools/        yaml2json.py (YAML→JSON), embed_data.py (bake data into flash), gen_font.py, gen_wiring_excalidraw.py
third_party/  vendored googletest + nlohmann/json
docs/         plan, architecture, domain, midi-protocol, config-format, mcu, wiring, wifi-app-handoff
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
- [docs/mcu.md](docs/mcu.md) — firmware build/flash, adapters, hardware status
- [docs/wiring.md](docs/wiring.md) — Pico 2 W pin map, parts, power (+ `wiring.excalidraw` diagram)
- [docs/wifi-app-handoff.md](docs/wifi-app-handoff.md) — editor-app protocol contract (WiFi, device discovery, firmware update)
