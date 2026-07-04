# Power Supply — USB + Pedalboard 9V

The MIDI controller needs to run from two sources without conflict:

- **USB 5V** while programming/debugging the Pico from a PC.
- **Pedalboard 9V** (regulated down to 5V) during normal use on the board.

The Pico's power input is designed for exactly this. You don't switch between
sources — you OR them together so whichever is higher wins, with no
back-feeding.

## The Pico power pins

- **VBUS (pin 40)** — raw 5V straight from the USB connector.
- **VSYS (pin 39)** — main system input (1.8–5.5V). Feeds the onboard
  buck-boost SMPS that generates the 3.3V rail.
- On-board there is a **Schottky diode (D1) from VBUS → VSYS**. USB power
  reaches VSYS through ~0.3V of diode drop, but VSYS can never push current
  *back* into VBUS / the USB host.

That internal diode is the whole trick: mirror it for the external supply.

## Recommended circuit — two-diode OR into VSYS

```
   USB 5V ──[ internal D1 ]──┐
                             ├──► VSYS (pin 39) ──► SMPS ──► 3V3
9V→5V module ──[ ext Schottky ]─┘
                  │
   pedalboard GND ── common GND ── Pico GND (pin 38)
```

1. Regulate pedalboard **9V down to ~5V** with the module below.
2. Feed that 5V into **VSYS through a Schottky diode** (~0.3V drop, matches
   the internal D1).
3. Tie **grounds together** (pedalboard supply GND ↔ Pico GND).

VSYS then sits at roughly `max(VBUS − 0.3, Vext − 0.3)`. Whichever source is
higher powers the board; the lower one is reverse-blocked by its diode. USB can
be plugged in to program **while** the 9V is connected, with zero contention.

## The 9V → 5V module

**Breadboard: Recom R-78E5.0-1.0** — a synchronous buck regulator with the
inductor potted inside the package, so there's no separate magnetics part to
source or lay out. It's a **leaded, through-hole SIP-3 part** (TO-220-compatible
pin spacing, 2.54mm pitch) — plugs straight into a breadboard.

**PCB: MPM3610** — a synchronous buck regulator with the inductor built into
the package, so there's no separate magnetics part, no missing footprint to
chase down, and no FB-divider/inductor/extra-cap parts to place. It accepts up
to 21V in, comfortably covering the 9V pedalboard rail.

| Part | Breadboard | PCB |
|---|---|---|
| 9V→5V module | [Recom R-78E5.0-1.0](https://www.digikey.com/en/products/detail/recom-power/R-78E5-0-1-0/4930585) — SIP-3, THT, 5V/1A out, up to 28V in | [MPM3610GQV-Z](https://www.digikey.com/en/products/detail/monolithic-power-systems-inc/MPM3610GQV-Z/5292909) — QFN-20, 3×5×1.6mm, 5V/1.2A out, up to 21V in |
| OR-ing diode into VSYS | [1N5817-T](https://www.digikey.com/en/products/detail/diodes-incorporated/1N5817-T/22052) (DO-41, THT) | [SS14](https://www.digikey.com/en/products/detail/onsemi/SS14/965474) (SMA/DO-214AC) |

### PCB footprints (KiCad)

| Part | Package | KiCad footprint |
|---|---|---|
| MPM3610GQV-Z | QFN-20, 3×5×1.6mm | *No stock KiCad footprint* — download from [SnapEDA](https://www.snapeda.com/parts/MPM3610GQV-Z/MPS/view-part/) |
| SS14 diode | SMA / DO-214AC | `Diode_SMD:D_SMA` |

## Important details

- **Stay under 5.5V at VSYS.** 5V minus a Schottky drop ≈ 4.7V — well in
  range. Never feed 9V directly to VSYS.
- **Do NOT connect external 5V straight to VBUS.** That ties it to the USB 5V
  line and can back-feed the PC's USB port. Always enter via VSYS through the
  diode.
- **Pedalboard polarity:** many 9V pedal supplies are center-negative
  (Boss style) and daisy-chains can share ground unexpectedly. Ensure the
  module's input ground = output ground = Pico ground, and confirm jack
  polarity before wiring.
- **Current budget:** Pico + SPI OLED + MCP23017 (@0x22) + the I²C→MIDI PIC
  bridge (@0x04) is well under ~300mA — the MPM3610's 1.2A rating leaves
  plenty of headroom.

## Pedalboard Isolation (Strymon Ojai)

Audio noise on a pedalboard couples in through **shared ground and shared
supply**, not through the regulator topology. The Pico already has its own
switching SMPS onboard (the buck-boost making 3.3V), so the board is a
switching-noise source regardless of how the 5V is made. The defense is
**isolation**, and the supply for this build handles it:

- **Strymon Ojai** — five fixed **9V / 500 mA fully isolated** outputs off a
  24W brick (24V adapter). Each output is transformer-isolated, so its ground
  does **not** share copper with the audio pedals → no ground loop, no
  conducted hum path into the audio chain. (The 9-output **Zuma**, 48W, behaves
  identically if used instead.)
- **Use one dedicated isolated output** for the controller. **Do not
  daisy-chain** it with an audio pedal — keep them on separate windings.
- **Current:** controller draws ~300 mA → ~2.7W at the 9V input. Well under the
  500 mA per-output limit. Keep a running tally of all pedals so the board stays
  within the **24W total** budget.
- **Polarity:** Strymon outputs are **center-negative** 2.1mm barrel
  (Boss-standard). Wire the input jack as **tip = − / ground, sleeve = +9V**.
  Don't assume center-positive.
