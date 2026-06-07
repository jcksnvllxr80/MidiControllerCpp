# Config format (JSON)

The firmware reads JSON. `tools/yaml2json.py` converts the original Python YAML
1:1 (preserving key order — `set_params` emits in declaration order, so the
loader uses `nlohmann::ordered_json`). Files live under `data/`:

```
data/midi_controller.json     device state + channel table + footswitch map
data/pedals/<Pedal>.json      one pedal's command map
data/songs/<Song>.json        a song: parts -> pedals -> state
data/sets/<Set>.json          an ordered list of song names
```

## `midi_controller.json` → `ControllerState`

```jsonc
{
  "button_setup": { "1": {"function":"Part Dn","long_press_func":"Select Part Dn"}, ... },
  "controller_api": {"buttons_locked": false, "port": 8090},
  "current_settings": {"mode":"standard", "preset":{"setList":"...","song":"...","part":"..."}, "tempo":106},
  "knob": {"color":"Yellow", "brightness":"100"},
  "midi": {"channels": {"1": {"name":"QuartzV2","state":true,"preset":{"number":2}}, "6": null, ... }}
}
```

Null channels are dropped; channels load in ascending order. A channel preset is
`number` if present, else `name` (so `{"name":"Plexi"}` is a name-based preset).

## `pedals/<Pedal>.json` → `PedalConfig`

The top-level keys are **groups**: `Engage`, `Bypass`, `Toggle Bypass`,
`Set Preset`, `Set Tempo`, `Knobs/Switches`, `Parameters`. Each group is an
**action node**; nested maps become child action nodes.

Recognised leaf keys on an action node:

| key | meaning |
|---|---|
| `cc` | CC number (note: `0` is treated as "absent" — see midi-protocol.md) |
| `program change` / `control change` | sub-action `{func?, min?, max?, options?}` |
| `multi` | `{ "1": "child", "2": "child" }` — ordered sub-actions to run |
| `func` | a `Transform` expression (below) |
| `dict` | name→int map (param value lookup) |
| `value`, `on`, `off`, `press`, `release`, `min`, `max` | scalar leaves |
| `notes`, `display`, `options` | metadata (ignored by the MIDI core) |

Any other key is a **named child** (e.g. `bank`/`preset` under `Set Preset`, or
each knob under `Knobs/Switches`).

### `Transform` grammar (replaces `eval`)

Only two shapes are accepted (anything else throws at load):

1. **Arithmetic** — `x <op> n`, `op ∈ + - * / %`. Integer division/modulo (Python-2
   semantics for the non-negative MIDI values used). e.g. `"x % 128"`.
2. **Dict lookup** — `{"Key": int, ...}.get(x, None)`; a missing key yields
   "None" (nothing sent). e.g. ScarlettLove preset selection.

## `songs/<Song>.json` → `Song`

```jsonc
{
  "name": "Victory - Rhythm",
  "tempo": 110,
  "parts": {
    "Chorus": {
      "position": 1,
      "pedals": {
        "TimeLine":     {"engaged": false, "preset": 2},
        "ScarlettLove": {"engaged": true,  "preset": "Plexi"},
        "SuperSwitcher":{"preset": 1, "params": {"Loop 1": {"name":"Morning Glory","engaged":false}, ...}},
        "Iridium":      {"engaged": true, "preset": 1, "params": null}
      }
    }
  }
}
```

Parts are ordered by `position`. A pedal preset may be an int or a name string;
an absent preset is a no-op. `params` is an ordered map of `name → value`, where
a value is a scalar, a string, or `{name, engaged}` (only `engaged` affects
MIDI). `null`/absent `params` means none.

## `sets/<Set>.json` → `Setlist`

```jsonc
{ "name": "2017-05-22 Set", "songs": ["Victory - Rhythm", "Here As In Heaven - Lead", ...] }
```

Songs load in listed order; each name resolves to `songs/<name>.json`.

## Re-running the converter

```sh
python3 tools/yaml2json.py [SRC_DIR] [DEST_DIR]
# SRC_DIR defaults to the original Python project; DEST_DIR defaults to ./data
```
