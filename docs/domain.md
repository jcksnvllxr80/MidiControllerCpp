# Domain model

The domain core is pure C++17 — no hardware, no JSON, no globals. Every module
maps to a piece of the original Python.

| C++ (`include/mc/domain`) | Ports from (Python) | Notes |
|---|---|---|
| `Value` | duck-typed `value` in EffectLoops | `variant<None, bool, long, string, EngagedRef>`. `EngagedRef{name, engaged}` is a song's `{name, engaged}` param value. |
| `MidiMessage` | `MIDI.py` | CC `[0xB0\|ch-1, cc, val]`, PC `[0xC0\|ch-1, prog]`. Channel→status math lives here, testable. |
| `Transform` | `EffectLoops.check_for_func` (`eval`) | Safe parser for `x <op> n` and `{...}.get(x, None)`. No `eval`. Unsupported expressions throw at load. |
| `PedalConfig` / `Action` | per-pedal YAML | Typed, recursive action tree. Group nodes (Engage, Set Preset, Knobs/Switches…) are the root `Action`'s children. |
| `MidiPedal` | `EffectLoops.MidiPedal` | `turnOn/Off`, `toggle`, `setPreset`, `setTempo`, `setParams`, `setSetting` + the dispatch (`determine_action_method` / `determine_parameter_method` / `convert_to_int` / `handle_multi`). Emits via `IMidiOut`. |
| `Setlist`/`Song`/`Part` | `PartSongSet.py` | Plain `std::vector` (the hand-rolled doubly-linked list is gone; prev/next is `index ± 1`). A `Part` holds, per pedal, `PedalState{engaged, preset, params, settings}`. |
| `ButtonSM` | `EffectLoops.ButtonOnPedalBoard` | Short (<0.5s) / long press; partner double-press (0.25s window). Time via `IClock`. |
| `ControllerState` | `midi_controller.yaml` | Current set/song/part, tempo, mode, knob colour/brightness, button lock, the channel→pedal table, the footswitch action map. |
| `MenuTree` | `N_Tree.py` + RotaryEncoder menu logic | The tree plus the rotary navigation engine and message formatting. Hardware-free: messages go to an injected sink, node behaviours are callbacks. |

## Deliberate differences from the Python

These are intentional, documented, and covered by tests.

- **Construction has no side effects.** Python's `MidiPedal.__init__` emitted
  engage + preset MIDI as a side effect of building the object. Here, building a
  pedal sends nothing; the `Application` drives every emission through
  `loadPart()`. Cleaner and deterministic.
- **Tempo is a first-class port.** `Application::loadPart()` always pushes the
  song BPM to `ITempoOut`. Python only did this when a pedal happened to be named
  `TapTempo` (none is), so tempo was effectively dead. The plan promotes it.
- **No `eval`.** `Transform` is a fixed grammar; an unknown expression is a loud
  load-time error rather than a silently-evaluated lambda.
- **Menu select uses clean semantics.** The deep "edit a pedal's CC map" submenu
  (the densest, most device-specific Python) is not ported; the navigation engine
  that powered it *is*, and the Set/Song/Part/Knob/Lock menus are wired and
  tested.
- **Defaults are not auto-persisted.** Python rewrote `midi_controller.yaml` on
  every part load. The sim does not (to avoid mutating the shared fixture);
  `IConfigStore::write` exists and is tested, ready to wire up.

See [`midi-protocol.md`](midi-protocol.md) for the byte-level dispatch and the
faithful quirks.
