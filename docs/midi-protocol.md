# MIDI protocol & dispatch

How a high-level action becomes exact MIDI bytes. This mirrors
`EffectLoops.MidiPedal` in the original Python, including a few load-bearing
quirks the tests lock in.

## Byte construction (`MidiMessage`)

- **Control Change:** `[0xB0 | (channel-1), cc, value]`
- **Program Change:** `[0xC0 | (channel-1), program]`

Channel is 1..16. Example: TimeLine is on channel 2, so engage (`cc:102`,
`value:127`) is `B1 66 7F`.

## `determine_action_method` (engage / bypass / toggle / set preset / set tempo)

Given an action node and a value (defaulting to the node's `value`):

1. **`cc` present and non-zero** → `value = func(value)`; send `CC(cc, value)`.
2. else **`program change`** → `value = func(value)`; send `PC(value)`.
3. else **`control change`** → `value = func(value)`; send `CC(value, 127)` —
   the computed value is the CC *number*, the data byte defaults to 127.
4. else **`multi`** → run each named sub-action in order (below).

The `func` is the node's `Transform` (identity if none).

### Quirk 1 — `cc: 0` is falsy

Step 1 says "non-zero" because Python's `if action_dict.get('cc'):` treats `0`
as falsy. The "Set Preset → bank" sub-action is `{cc: 0, func: "x / 128"}`, so
**the bank select CC is never sent** — only the `preset` Program Change goes out.
We reproduce this exactly (`Action::hasTruthyCc()`), so presets ≤ 127 work and
the bank byte is omitted just like on the real rig.

### Quirk 2 — `multi` mutates the threaded value

`handle_multi` threads one `val` through every sub-action and reassigns it each
step. For TimeLine's `Set Preset` the bank is skipped (Quirk 1), so `preset`
sees the original value and sends `PC(value % 128)`. For QuartzV2's `Set Tempo`
(`hundreds: x/100` then `tens_ones: x%100`, both `cc:94`), the second step
operates on the **already divided** value — so tempo 110 sends `CC#94=1` then
`CC#94=1`, not `1` then `10`. Faithful to the Python; locked by a test. (Tempo is
never actually triggered on the rig — no pedal is named `TapTempo` — so this is
latent, but preserved.)

## `determine_parameter_method` + `convert_to_int` (knob/param tweaks)

Params (`Knobs/Switches`, `Parameters`) only fire when `cc` is non-zero. The
value is resolved by `convert_to_int`, in this order (matching Python):

1. a direct `on`/`off`/`press`/`release`/`value` key, or a `dict` entry → that int
2. `value == v` → `int(v)`
3. a bool (`EngagedRef` collapses to its `engaged` flag) → `on` / `off`
4. an int within `[min, max]` → itself (out of range → not sent)

Example: SuperSwitcher `Loop 1` is `{cc:102, on:127, off:0}`; a song's
`{name:"Morning Glory", engaged:false}` → `CC#102 = 0`.

## Per-part emission order

`Application::loadPart()` iterates pedals **in channel order** (QuartzV2=1,
TimeLine=2, Iridium=3, BigSky=4, SuperSwitcher=5, ScarlettLove=16), and per pedal
does engage/bypass → preset → params → settings. `tests/e2e` pins the full
sequence for a real part:

```
B0 61 00   QuartzV2 preset 0
B1 66 00   TimeLine bypass        C1 02   TimeLine preset 2
B2 66 7F   Iridium engage         C2 01   Iridium preset 1
B3 66 7F   BigSky engage          C3 01   BigSky preset 1
C4 01      SuperSwitcher preset 1
B4 66 00 … B4 71 00   SuperSwitcher Loop/Control params
BF 17 01   ScarlettLove engage    BF 15 7F   ScarlettLove preset Plexi (dict lookup → cc21)
```
