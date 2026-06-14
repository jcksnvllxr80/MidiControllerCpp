# MidiController: Python → C++ Port

## Context
The guitar pedal runs as Python on a Raspberry Pi 4: 6 footswitches, an OLED, a rotary
knob (RGB LED + pushbutton), 2 MIDI outputs, 4 tempo 1/4" outputs. Today the Pi is the
"brain" (UI, song/preset state, MIDI message building) and an Arduino Pro Micro (I2C) does
the actual MIDI bytes + tempo pulses. A Java HTTP bridge (`piMidiHttp`) + a web app edit
config files over WiFi.

Goal now: rewrite the **brain** in clean, portable C++ so it can later move to a
microcontroller (WiFi → USB). Get the C++ patterns right and fully tested first; hardware
comes later. This plan covers the firmware only; the controller/editor app is a separate project.

## Decisions (locked)
- **Firmware lives in** `C:\Users\A-A-Ron\git\MidiControllerCpp\MidiControllerCpp`. The controller/editor app is a separate project, out of scope here.
- **Config format: JSON.** One-time convert the existing YAML songs/sets/pedals to JSON.
- **Build/run: WSL + GoogleTest, plain Makefile.** Host: `make build-sim` / `make test` / `make run` (only needs `g++`). Firmware: `make build` → the Pico `.uf2` (needs the Pico SDK). Phase 1 had no real hardware — desktop/sim adapters only.
- **Architecture: ports & adapters (hexagonal).** Pure C++ domain core; all hardware behind interfaces. Same core runs on the sim now and the microcontroller later.
- **This plan is maintained in-repo** at `MidiControllerCpp/docs/plan.md` as the canonical, living roadmap — updated as each phase lands. The `~/.claude/plans/` copy is throwaway.

## Status (updated 2026-06-14)
- **Phase 1 — DONE.** Domain core (`MidiMessage`, `Value`, `Transform`, `PedalConfig`,
  `MidiPedal`, `Setlist/Song/Part`, `ButtonSM`, `ControllerState`, `MenuTree`), all 8
  ports, JSON config loaders, and the one-shot `tools/yaml2json.py` → committed `data/*.json`.
- **Phase 2 — DONE.** Sim adapters (`adapters/sim`), the `Application` event loop + `main.cpp`,
  and docs (`README.md` + `docs/{architecture,domain,midi-protocol,config-format}.md`).
- **Tests — ~850 passing** (unit + mock + e2e) via vendored GoogleTest/GoogleMock; `make build-sim`/`test`/`run`
  all green in WSL, zero warnings. Verification criteria 1–4 below are met. Coverage: a data-driven
  suite checks every reachable pedal param (representative values) through the real dispatch, plus
  per-module suites for MidiMessage/Value/Transform/PedalConfig/MidiPedal/SongPartSet/ControllerState/
  ButtonSM/MenuTree/ConfigLoader/FsConfigStore/sim adapters/Application, and per-part e2e flows.
- **Faithful-port decision:** observable MIDI is reproduced byte-for-byte, including the Python
  quirks (notably `Set Preset`'s `bank` uses `cc:0`, falsy in Python → bank select never sent;
  only the preset Program Change goes out). Documented in `docs/midi-protocol.md` and locked by tests.
- **Phase 3 (mcu) — builds clean, under hardware bring-up.** Target board: **Pico 2 W (RP2350)**.
  `adapters/mcu/` + the Pico `CMakeLists.txt` + `tools/embed_data.py` build a ~1.5 MB `.uf2` (`make build`);
  the core is MCU-ready (`Transform`'s `std::regex` is a hand parser; nlohmann/json is kept — the RP2350's
  520 KB RAM affords it). All adapters, WiFi (CYW43 + lwIP + mDNS) with non-blocking auto-reconnect,
  `FlashKv` persistence via `flash_safe_execute`, an 8 s watchdog, a boot-config guard, debounced
  "save defaults", the full OLED font, and the editor protocol (incl. `delete_*`/`write_part`,
  `reboot`/`reboot_bootloader`, `device_id`) are **compile/link-verified**. The **USB serial editor
  link is confirmed on hardware**; WiFi and the physical I/O are not yet bench-tested. See `docs/mcu.md`.

## What it does (so the model is clear)
Data model: **Setlist → Songs → Parts**. A Part stores, per pedal, `(engaged, preset, params, settings)`. Loading a Part sends MIDI to set every pedal's on/off + preset (+ tempo). A **MidiPedal** has a name, a MIDI channel (1–16), and a per-pedal command map that turns high-level actions (Engage, Bypass, Set Preset, Set Tempo, knob/param tweaks) into MIDI CC/PC bytes. Footswitches step parts/songs; the rotary knob drives a menu for editing.

## Architecture
```
        USB IConfigTransport  (external editor — separate project)
                 │
        ┌────────▼─────────┐
        │   Ports (HAL)    │  IMidiOut ITempoOut IDisplay ILed
        │   interfaces     │  IInput IClock IConfigStore IConfigTransport
        └───┬──────────┬───┘
   adapters │          │ adapters
   ┌────────▼───┐  ┌───▼──────────┐
   │ sim/desktop│  │ mcu (Phase3) │
   └────────────┘  └──────────────┘
        ▲
   ┌────┴───────────────────────────────────┐
   │ Domain core (pure C++, no hardware)     │
   │ MidiPedal · Transform · Setlist/Song/   │
   │ Part · ControllerState · ButtonSM ·     │
   │ MenuTree · MidiMessage                  │
   └─────────────────────────────────────────┘
```

## Target layout (`MidiControllerCpp/`)
```
include/           public headers (mirror src)
src/
  domain/          MidiMessage, MidiPedal, PedalConfig, Transform,
                   Setlist/Song/Part, ControllerState, ButtonSM, MenuTree
  ports/           IMidiOut, ITempoOut, IDisplay, ILed, IInput, IClock, IConfigStore,
                   IConfigTransport, IWifi, ISystemControl  (pure virtual)
  config/          JSON loaders -> domain objects
  adapters/sim/    console display, scripted input, fs config store, chrono clock, logging midi
  adapters/mcu/    Pico 2 W: GPIO/OLED/MIDI-DIN/tempo/LED, FlashKv, WiFi, USB editor link
  app/             Application (composition root + event loop)
  main.cpp
data/              converted JSON: midi_controller.json, songs/, sets/, pedals/
tests/             unit/, mock/, e2e/ + fixtures
docs/              plan.md (this roadmap), architecture.md (mermaid), domain.md, midi-protocol.md,
                   config-format.md, mcu.md, wiring.md (+ wiring.excalidraw), wifi-app-handoff.md
third_party/googletest   (vendored submodule, built by Makefile)
tools/             yaml2json.py (YAML→JSON), embed_data.py (bake → flash), gen_font.py, gen_wiring_excalidraw.py
Makefile
```

## ~~Phase 1 — Domain core + config (the actual conversion)~~ ✅ done
Build the pure-C++ core. Python source → C++ target:

| C++ module | Ports from | Notes |
|---|---|---|
| `MidiMessage` | `MIDI.py` | Builds CC `[0xB0|ch-1, cc, val]` / PC `[0xC0|ch-1, prog]`. Channel→status math lives here (testable), not in the adapter. |
| `Transform` | `EffectLoops.check_for_func` (`eval('lambda x: ...')`) | Replace `eval` with a tiny safe parser for `x <op> n`, op ∈ `+ - * / %`. Covers the only real cases (`x / 128`, `x % 128`). |
| `PedalConfig` | per-pedal YAML (`TimeLine.yaml`…) | Parsed map: Engage/Bypass/Toggle/Set Preset/Set Tempo/Knobs-Switches/Parameters. Each action = cc|pc|multi + optional transform, `dict` (name→int), `min`/`max`, `on`/`off`. |
| `MidiPedal` | `EffectLoops.MidiPedal` | `turnOn/Off`, `toggle`, `setPreset`, `setTempo`, `setParam(s)`, `setSetting`. Same dispatch as `determine_action_method` / `convert_to_int` / `handle_multi_functions`. |
| `Setlist/Song/Part` | `PartSongSet.py` | Plain `std::vector` (drop the hand-rolled doubly-linked list; keep prev/next via index). Part = `map<pedalName, PedalState>`. |
| `ButtonSM` | `EffectLoops.ButtonOnPedalBoard` | Short (<0.5s)/long press; partner double-press 0.25s window. Time via `IClock` so it's testable. |
| `ControllerState` | `midi_controller.yaml` | current setlist/song/part, tempo, mode, knob color/brightness, buttons_locked. |
| `MenuTree` | `N_Tree.py` + `RotaryEncoder.py` menu logic | The messiest port. Rotary CW/CCW navigation + select/long-hold thresholds. Port the tree + actions; isolate from hardware. |
| `config/*` | `setup()` in `midi_controller.py` | Load JSON → build pedals/state/songs. |

Ports defined this phase (headers only, sim impls in Phase 2): `IMidiOut`, `ITempoOut`,
`IDisplay`, `ILed`, `IInput`, `IClock`, `IConfigStore`, `IConfigTransport`.

Data: run `tools/yaml2json.py` once to convert all existing YAML (songs, sets, pedals,
`midi_controller.yaml`) into `data/*.json`; commit the JSON. JSON lib: vendor a single
header (e.g. `nlohmann/json`) now; swap to a tiny MCU-friendly parser in Phase 3.

## ~~Phase 2 — Simulator, event loop, e2e, docs~~ ✅ done
- **Sim adapters** (`adapters/sim/`): console/stdout display, scripted-or-keyboard input, `std::filesystem` config store, `std::chrono` clock, MIDI/tempo adapters that log messages.
- **Application** (`app/`): wires ports+adapters and runs the event loop — port of `main()`/`setup()`. Poll input → button/encoder handler → mutate state → update display → emit MIDI/tempo. Replaces the Flask loop.
- **`ITempoOut` made explicit**: the 4 tempo outputs + tap tempo were Arduino-only before. Define the port now (set BPM / emit tap); sim adapter logs.
- **Docs**: `architecture.excalidraw` (the diagram above), root `README.md` (build/test + layout, mermaid sequence of the event loop), short `domain.md`, `midi-protocol.md`, `config-format.md`. Concise — only what must be understood.

## Phase 3 — Microcontroller (in progress — Pico 2 W / RP2350)
Swap in `adapters/mcu/`: real GPIO for footswitches/encoder/LED, SSD1306 driver, **native** MIDI DIN (2×) and the 4 tempo 1/4" outputs (absorbing the Arduino's job), flash/SD `IConfigStore`, and **USB `IConfigTransport`**. Core and tests stay unchanged — that's the payoff of the HAL.

Done (compile/link-verified, `make build` → `.uf2`): all adapters (`McuClock/McuMidiOut/TeeMidiOut/McuTempoOut/McuLed/McuInput/Ssd1306Display/McuConfigStore/FlashKv/StdioConfigTransport`), `Pins.h`, `main_mcu.cpp`, the Pico `CMakeLists.txt` (board `pico2_w`, exceptions on), and `tools/embed_data.py`. Added beyond the original scaffold: **WiFi** (`WifiManager` — CYW43 STA + lwIP TCP `:8080` + mDNS, the same `EditorProtocol`, non-blocking auto-reconnect), the full OLED font (`tools/gen_font.py`), `FlashKv` persistence via `flash_safe_execute` (WiFi creds + debounced "save defaults"), an 8 s **watchdog**, a boot-config guard, editor-protocol `delete_*`/`write_part` + `reboot`/`reboot_bootloader` + `device_id` (`ISystemControl`/`McuSystemControl`), and the raw-USB/WinUSB link (`-DMC_ENABLE_USB_EDITOR`). `std::regex` removed from `Transform`. The desktop Makefile excludes `src/adapters/mcu/`; SDK-free MCU logic (codec, config store) is host-tested.

TODO before it ships: bench-test the physical I/O (footswitches, encoder, OLED, MIDI DIN, tempo, LED) and WiFi on a real board; an on-target RAM check parsing BigSky; the deferred on-device features (deep "Midi Pedals" editor menu, favourite mode, footswitch partner combos, About/IP screen, tap tempo); and a deliberate later pass on WiFi at-rest cred scrambling + wire auth/TLS. See `docs/mcu.md`.

## Tests (GoogleTest / GoogleMock)
- **Unit** (`tests/unit/`): `Transform` parse+eval; `MidiMessage` byte exactness; `convert_to_int`/dict/min-max/on-off; Setlist/Song/Part JSON load; `ButtonSM` short/long/partner with a fake clock; `MenuTree` navigation; config loaders.
- **Mock** (`tests/mock/`): `MidiPedal` against `MockMidiOut` — assert the **exact** CC/PC byte sequence for engage/bypass/preset (incl. `multi` bank+preset = `x/128` then `x%128`)/tempo/param. `ButtonSM` against `MockClock`. `Application` against mock display/input — assert display text.
- **E2E** (`tests/e2e/`): drive `Application` with scripted input + fake clock + the **converted real** fixtures (a setlist with real pedals). Assert the MIDI/tempo output sequence and display messages across: load setlist → next/prev part → next song → tap tempo → menu edit. This is the in-process equivalent of the old Flask `short`/`long`/`dpad` endpoints.

## Build (Makefile, run in WSL)
- `make build` — build the Pico firmware `.uf2` (CMake + Pico SDK; see `docs/mcu.md`).
- `make build-sim` — compile core + sim into a `midicontroller` binary.
- `make test` — build & run all GoogleTest suites (gtest/gmock compiled from vendored `third_party/googletest`).
- `make run` — launch the sim.
- `make clean` / `make pico-clean` — remove the desktop `build/` / firmware `build-pico/` artifacts.
Toolchain: `g++` (C++17), no apt deps beyond a compiler — googletest and the JSON header are vendored.

## Key risks / explicit decisions
- **`eval` → `Transform`**: arbitrary Python lambdas become a fixed safe grammar. If a pedal config ever needs an unsupported expression, the loader errors loudly rather than guessing.
- **Menu tree** is the densest Python logic — port it behind tests first, with `MenuTree` fully hardware-free.
- **MIDI channel math** stays in the domain so output is verifiable without hardware.
- **Tempo outputs** become a first-class port now, even though Phase-1 only logs them.

## Verification
1. `make build-sim` succeeds in WSL.
2. `make test` — all unit/mock/e2e suites green; mock tests prove exact MIDI bytes match the Python behavior for real pedal configs.
3. `make run` — sim boots from `data/`, scripted footswitch/encoder input changes parts/songs and logs the expected MIDI + display text.
4. Spot-check: converted `data/*.json` round-trips the same pedal/preset/tempo state the Pi used (`midi_controller.yaml` → `midi_controller.json`).
