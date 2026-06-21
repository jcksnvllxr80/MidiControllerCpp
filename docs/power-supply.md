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
9V→5V reg ──[ ext Schottky ]─┘
                  │
   pedalboard GND ── common GND ── Pico GND (pin 38)
```

1. Regulate pedalboard **9V down to ~5V**.
2. Feed that 5V into **VSYS through a Schottky diode** (e.g. 1N5817 / SS14,
   ~0.3V drop to match the internal D1).
3. Tie **grounds together** (pedalboard supply GND ↔ Pico GND).

VSYS then sits at roughly `max(VBUS − 0.3, Vext − 0.3)`. Whichever source is
higher powers the board; the lower one is reverse-blocked by its diode. USB can
be plugged in to program **while** the 9V is connected, with zero contention.
This is the method in the Raspberry Pi Pico datasheet ("Powering Pico").

## Important details

- **Stay under 5.5V at VSYS.** 5V minus a Schottky drop ≈ 4.7V — well in range.
  Never feed 9V directly to VSYS.
- **Use a buck (switching) regulator, not a 7805**, if heat matters. 9V→5V
  linear burns `4V × load` as heat. An MP1584 / mini-360 buck module is cheap
  and runs cool. A 7805 works but gets warm (needs ~1.5–2V dropout headroom).
- **Current budget:** Pico + SPI OLED + MCP23017 (@0x22) + the I²C→MIDI PIC
  bridge (@0x04) is well under ~300mA, so a small 5V regulator is plenty.
- **Pedalboard polarity:** many 9V pedal supplies are center-negative
  (Boss style) and daisy-chains can share ground unexpectedly. Ensure the
  regulator's input ground = output ground = Pico ground, and confirm jack
  polarity before wiring.
- **Do NOT connect external 5V straight to VBUS.** That ties it to the USB 5V
  line and can back-feed the PC's USB port. Always enter via VSYS through the
  diode.

## Lower-drop alternative

To avoid the ~0.3V Schottky loss, replace the external diode with a P-channel
MOSFET ideal-diode / load-sharing IC (e.g. LM66100 into VSYS). Same OR-ing
behavior, near-zero drop. The plain Schottky is simpler and fine here, since a
~4.7V VSYS is plenty for the SMPS.

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

Because isolation kills the ground-loop path, the regulator choice below is only
about local ripple on the 5V rail — which the Pico's SMPS rejects anyway. Either
a filtered buck or a linear is safe here.

## Regulate Pedal Power Down to 5V

Use a **synchronous buck (step-down switching) regulator**. Linear regulators
(7805 / LM317) dump the extra voltage as heat — at 9→5V they're only ~55%
efficient. A good synchronous buck hits **90–95%**.

### Why buck wins here

At 9V in, 5V out, ~300mA load (~1.5W):

- **7805 linear:** dissipates `(9 − 5) × 0.3 = 1.2W` as heat → ~55% efficient,
  runs hot, no heatsink margin.
- **Synchronous buck:** dissipates ~0.1–0.15W → ~90–95% efficient, stays cool,
  no heatsink.

For a pedalboard supply that's often current-limited, the efficiency also means
you draw less from the 9V rail.

### Options

| Option | Topology | Efficiency | Notes |
|---|---|---|---|
| **TPS562201 / TPS563201** | Synchronous buck IC | ~90–95% | Best pick for a PCB layout. Fixed-freq, tiny, cheap, low Iq. Needs 1 inductor + a few caps. |
| **MP2315 / MP1584EN** | Async/sync buck IC | ~88–93% | MP1584 is the classic "mini-360" module — works but asynchronous (slightly less efficient, pot-adjustable). |
| **Pre-made buck module** (mini-360 / MP1584) | Module | ~88–92% | Fastest path. Set the pot to 5.0V *before* wiring to the Pico. Cheap, but QC varies — measure output. |
| **R-78E5.0-1.0** (Recom) | Drop-in switching reg | ~90% | Pin-compatible with a 7805 (TO-220 footprint), no external parts. Easiest reliable swap. |

### Recommendation

- **Zero design effort / drop-in:** the **Recom R-78E5.0-1.0** — same 3 pins as
  a 7805, no inductor/caps to add, ~90% efficient, handles 1A. Just drop it into
  a linear-reg footprint.
- **Proper PCB layout:** a **TPS562201** synchronous buck. Best
  efficiency-per-cost, runs cold, plenty of headroom over the ~300mA budget.

### Practical notes

- **Size for headroom:** load is ~300mA; pick a reg rated ≥1A so it loafs and
  stays efficient/cool.
- **Input/output caps matter:** follow the datasheet (typ. 10µF in, 22µF out,
  low-ESR ceramic). The module options already include these.
- **Switching noise:** bucks put ripple/EMI on the rail. Add a small LC or
  10–22µF ceramic at the Pico's VSYS-side diode, and keep the inductor loop
  tight. Usually a non-issue for digital + OLED + MIDI, but worth a bulk cap.
- **Then feed VSYS through the Schottky** as above — the buck's 5.0V minus ~0.3V
  ≈ 4.7V at VSYS, in range.
