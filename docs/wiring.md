# Wiring — Pico 2 W (RP2350)

This is a **driving-board swap**, not a breadboard build. The MidiController PCB is
unchanged: the Raspberry Pi 4 that used to plug into the board's 40-pin header
(`J35` in `PCB/MidiControllerPCB.sch`) is replaced by a **Pico 2 W**, and the old
ribbon lands on a small adapter that routes each net to the Pico GPIO below.
**The GPIO assignments are defined in
[`include/mc/adapters/mcu/Pins.h`](../include/mc/adapters/mcu/Pins.h) — that header
is the source of truth.** A visual version is in
[`wiring.excalidraw`](wiring.excalidraw) (regenerate with
`python3 tools/gen_wiring_excalidraw.py`). The net-by-net mapping from the Pi's BCM
pins is in [`pi4-pico2w-conversion.md`](pi4-pico2w-conversion.md).

All logic is **3.3 V**. Nothing here is 5 V tolerant — do not feed 5 V into any GPIO.

> ⚠️ **The PCB's own chips run at 5 V, and six of the Pico pins below are inputs
> driven from that 5 V rail.** The board's `VCC` net (originally the Pi's 5 V
> pin, now fed from the Pico's `VBUS`) powers the MCP23017 (`U2`) and the PIC
> MIDI bridge (`U10`), and several passive networks are pulled up to that same
> `VCC` net — confirmed against the schematic netlist
> (`pcb/midi-controller/MidiControllerPCB.net`), not assumed.
>
> | GP | Signal | Why it's 5 V |
> |---|---|---|
> | GP4 | I²C SDA | Pulled to `VCC` by `R74` (4.7 kΩ); driven by `U2`/`U10`, both VDD = `VCC` |
> | GP5 | I²C SCL | Pulled to `VCC` by `R73` (4.7 kΩ); same drivers |
> | GP2 | MCP INT A | Push-pull output from `U2`, powered from `VCC` |
> | GP3 | MCP INT B | Same |
> | GP14 | Encoder A | `R29` (10 kΩ) pulls the switch node to `VCC`, then `R27` (10 kΩ) feeds the GPIO — idles at ~5 V, pulled low on contact closure |
> | GP15 | Encoder B | Same circuit via `R30`/`R28` |
>
> **`VCC` stays at 5 V — don't "fix" this by moving it to `3V3(OUT)`.** `U5`
> (a CD4050 hex buffer, `VCC`-powered) fans the PIC's `MIDI_Tx` line out to
> all 6 jacks; MIDI's current-loop spec is nominally 5 V through 220 Ω
> resistors, so dropping `VCC` to 3.3 V would weaken the current-loop drive to
> real MIDI gear. **Level-shift the Pico side instead** — see §3a below.
>
> The OLED SPI lines (GP17–GP21) are safe as-is — the OLED's `VEE` net is tied
> to `3V3(OUT)` (pin 36), not `VCC`, and the Pico drives those lines rather
> than receiving them.
>
> **The knob RGB LED is also wired to `VCC` (5 V), common-anode** — the
> rotary encoder part (`SW6`, `RGB_Rotary_Enc_PB`) has its LED-common pin 8
> tied to `VCC`, not GND. This doesn't overvolt GP10–GP12 in normal operation
> (the Pico drives those pins, so it controls the voltage rather than the LED
> imposing one), but it does mean the polarity assumed in §3 below and in
> `McuLed` (common-cathode) is backwards for the real board — construct
> `McuLed` with `commonAnode = true`.

---

## 1. What the Pico drives (the 14 ribbon signals)

Everything the Pico touches is one of these. The footswitches and the six MIDI
jacks are **not** wired to the Pico — they hang off the MCP23017 expander and the
PIC MIDI bridge, which the Pico reaches over I²C.

| GP | Pico pin | Peripheral | Signal | Notes |
|-----:|---------:|------------|--------|-------|
| GP4  | 6  | I²C0 SDA | shared I²C data | → MCP23017 `0x22` **and** PIC `0x04` |
| GP5  | 7  | I²C0 SCL | shared I²C clock | 400 kHz |
| GP2  | 4  | GPIO in  | MCP INT A | bank-A change (active-high) |
| GP3  | 5  | GPIO in  | MCP INT B | bank-B change |
| GP18 | 24 | SPI0 SCK | OLED SCLK | |
| GP19 | 25 | SPI0 TX  | OLED MOSI | write-only (no MISO) |
| GP17 | 22 | GPIO out | OLED CS | |
| GP20 | 26 | GPIO out | OLED DC | data/command |
| GP21 | 27 | GPIO out | OLED RST | reset |
| GP14 | 19 | GPIO in  | Rotary encoder A | internal pull-up |
| GP15 | 20 | GPIO in  | Rotary encoder B | internal pull-up |
| GP10 | 14 | PWM      | Knob LED Red | active-low duty |
| GP11 | 15 | PWM      | Knob LED Green | |
| GP12 | 16 | PWM      | Knob LED Blue | |

Plus **3V3(OUT) = pin 36**, **VBUS = pin 40** (5 V USB), and **GND** (pins 3, 8, 13,
18, 23, 28, 38). Free for expansion: GP0, GP1, GP6–GP9, GP13, GP16, GP22, GP26–GP28.

> I²C0 is valid on this SDA/SCL pair (SDA on 0/4/8/12/16/20, SCL on 1/5/9/13/17/21);
> SPI0 SCK is valid on GP18 and TX on GP19. Keep these on capable pins if you move
> them in `Pins.h`.

---

## 1a. Level-shifting the 5 V lines (GP2, GP3, GP4, GP5, GP14, GP15)

This bus runs I²C at **400 kHz Fast-mode** (`main_mcu.cpp`: `i2c_init(i2c0,
400'000)`). A generic BSS138 shifter is a passive RC pull-up design — its
low→high rise time depends on pull-up resistance × bus capacitance, and
Fast-mode's 300 ns rise-time budget is routinely missed by that kind of
shifter once a shifter board and adapter wiring add capacitance. So **SDA/SCL
need a real I²C-rated part; the other 4 lines don't** — split the level
shifting across two small parts instead of one:

**SDA/SCL (GP4/GP5) → PCA9306** (or TCA9406) — an active dual-supply I²C/SMBus
voltage-level translator, datasheet-rated for both 100 kHz and 400 kHz:

```
VREF1 → 3.3V (Pico 3V3(OUT), pin 36)     VREF2 → 5V (board VCC)
SDA1  → Pico GP4 (+ 2.2–4.7kΩ pull-up to VREF1)   SDA2 → board SDA node (at R74)
SCL1  → Pico GP5 (+ 2.2–4.7kΩ pull-up to VREF1)   SCL2 → board SCL node (at R73)
GND   → common GND
EN    → tie to VREF1 (3.3V) — do NOT leave floating (chip stays disabled if EN floats)
```

The discrete 3.3V-side pull-ups are still required here even though
`main_mcu.cpp` already calls `gpio_pull_up()` on GP4/GP5 — the Pico's internal
pull-up is a weak ~50–80 kΩ, sized for a slow direct 3.3V bus, not strong
enough to hit Fast-mode's 300 ns rise-time budget. Keep the internal pull
enabled (harmless) but don't rely on it alone; add real 2.2–4.7 kΩ resistors
on SDA1/SCL1. The 5V side keeps its existing `R73`/`R74` — no change there.

**MCP INT A/B + Encoder A/B (GP2, GP3, GP14, GP15) → 4× discrete BSS138,
no added pull-ups.** These are simple sub-1 kHz digital signals (interrupt
edges, encoder clicks) — nowhere near a speed regime where BSS138's RC
charging matters, and `McuInput.cpp` already configures the Pico side with
`gpio_pull_up()` (GP14/GP15) / `gpio_pull_down()` (GP2/GP3). Since these
signals only ever flow board → Pico, that existing internal pull is enough to
terminate the low-voltage side of the shifter — no need to duplicate the
resistor pairs a generic pre-made BSS138 module bundles in. The 5V side is
already terminated too (`R29`/`R30`, and the MCP's own push-pull drive), so
the whole section is just:

```
HV side (5V)                        LV side (3.3V)
─────────────                       ──────────────
GND → board GND                     GND → Pico GND
(no HV or LV supply pin needed — no pull-ups to bias)

BSS138 #1: drain → MCP INT A         source → Pico GP2
BSS138 #2: drain → MCP INT B         source → Pico GP3
BSS138 #3: drain → Encoder A (post-R27 node)   source → Pico GP14
BSS138 #4: drain → Encoder B (post-R28 node)   source → Pico GP15
(gate of all 4 → Pico 3V3(OUT))
```

A passive 2-resistor divider (e.g. 10 kΩ / 15 kΩ → ~3.0 V) per line is an
equally valid alternative to the bare BSS138s for these 4 lines only — just
never use a divider on SDA/SCL, since it can't pull the open-drain bus low
from either side.

`VCC` itself stays sourced from `VBUS` (5 V) as in the pinout table below —
only the Pico-facing side of these 6 signals moves to 3.3 V, through the two
shifters above.

### Parts — where to buy

| Part | Breadboard (THT/module) | PCB (SMD) |
|---|---|---|
| I²C level translator | [SparkFun Level Translator Breakout – PCA9306](https://www.sparkfun.com/sparkfun-level-translator-breakout-pca9306.html) | [PCA9306DCUR](https://www.digikey.com/en/products/detail/texas-instruments/PCA9306DCUR/768359) (VSSOP-8, TI) |
| I²C pull-ups (2.2–4.7 kΩ, on SDA1/SCL1) | included on the breakout above | [RMCF0603FT4K70](https://digikey.com/product-detail/en/RMCF0603FT4K70/RMCF0603FT4K70CT-ND/1943033), 4.7 kΩ, 0603, 1% |
| 4-channel logic level shifter | [Adafruit 4-channel I2C-safe Bi-directional Logic Level Converter (BSS138)](https://www.adafruit.com/product/757) | 4× [BSS138 (onsemi, SOT-23-3)](https://www.digikey.com/en/products/detail/onsemi/BSS138/244210) |

### PCB footprints (KiCad standard library names)

| Part | Package | KiCad footprint |
|---|---|---|
| PCA9306DCUR | VSSOP-8, 3.0×3.0mm, 0.65mm pitch | `Package_SO:VSSOP-8_3.0x3.0mm_P0.65mm` |
| RMCF0603FT4K70 (I²C pull-ups) | 0603 (1608 metric) | `Resistor_SMD:R_0603_1608Metric` |
| BSS138 (onsemi) | SOT-23-3 | `Package_TO_SOT_SMD:SOT-23` |

All three are in KiCad's stock footprint libraries — no download needed.
Double-check the PCA9306DCUR pin-1 orientation against the TI datasheet before
finalizing the silkscreen; VSSOP-8 pin-1 marking conventions vary by
manufacturer.

The Adafruit BSS138 breakout already includes its own 10 kΩ pull-ups on both
sides, which is harmless here even though the discrete-BSS138 SMD design above
doesn't need them — the module is a fine drop-in for breadboarding regardless.

---

## 2. What lives on the (unchanged) PCB, behind the buses

You don't wire these — they're already on the board and the Pico talks to them over
the shared I²C bus. They are configured **exactly** as the Pi configured them.

### MCP23017 expander — I²C `0x22`: footswitches + rotary push button
The 5 footswitches and the rotary encoder's push button are inputs on the expander.
`McpExpander` writes the same register set the Pi did (`Footswitches.py`):
IODIR/GPPU/GPINTEN/INTCON/DEFVAL/IPOL/IOCON. Bit map:

```
bit 0  (A0) = Footswitch 1     bit 8  (B0) = Footswitch 4
bit 1  (A1) = Footswitch 2     bit 10 (B2) = Footswitch 5
bit 2  (A2) = Footswitch 3     bit 15 (B7) = rotary push button (IPOL-inverted)
```

The expander's INT A/B outputs go to GP2/GP3; the firmware reads the bank GPIO over
I²C only when an INT line is asserted (the read clears it). Active-low footswitches
(pressed = 0); the B7 selector is inverted via IPOL so it reads pressed = 1.

### PIC18F26K80 MIDI bridge — I²C `0x04`: all MIDI out
The Pico sends **raw MIDI bytes** to the PIC over I²C (one byte per transaction, like
the Pi's `writeRaw8`). The PIC fans every byte out to all **6 jacks: 2× DIN-5 +
4× TRS-MIDI**. The Pico has no MIDI UART, and the TRS jacks carry **MIDI**, not tempo.

---

## 3. Per-device wiring (the Pico-side peripherals)

### OLED — SSD1306 128×64, SPI0 (7-pin module)
The PCB's OLED header (`J1`) is **7 wires**: the 5 logic lines below plus VEE (power)
and GND. The "4-wire SPI" interface mode = SCLK + MOSI + DC + CS; RST and power are
the other three. All five logic lines are firmware-driven:
```
OLED VEE  → 3V3 (pin 36)        OLED SCLK → GP18 (pin 24)
OLED GND  → GND                 OLED MOSI → GP19 (pin 25)
OLED CS   → GP17 (pin 22)       OLED DC   → GP20 (pin 26)
OLED RST  → GP21 (pin 27)
```
The firmware pulses RST on `begin()`, then drives commands (DC low) and pixel data
(DC high) at 10 MHz. This is the bus the original panel used.

### Rotary encoder — quadrature (the menu knob)
```
Encoder A      → GP14 (pin 19)   — via R27 (10k), pulled up to VCC (5V) by R29 (10k)
Encoder B      → GP15 (pin 20)   — via R28 (10k), pulled up to VCC (5V) by R30 (10k)
Encoder common → GND
```
**Not an internal-pull-up circuit** — the board pulls both lines up to the 5 V
`VCC` net, so they idle at ~5 V and pull low on contact closure. Route both
through the level shifter in §1a before wiring to the Pico. 4 quadrature steps
per detent (`kStepsPerDetent = 4`). **The push button is not here** — it's MCP
bit B7, read over I²C with the footswitches.

### Knob RGB LED — PWM, common-anode (confirmed on the real board)
```
GP10 → 220–470 Ω → LED Red cathode
GP11 → 220–470 Ω → LED Green cathode
GP12 → 220–470 Ω → LED Blue cathode
LED common anode → VCC (5V)
```
The encoder part's LED-common pin is tied to the board's `VCC` net (5 V), not
GND — confirmed in the schematic netlist. Construct `McuLed` with
`commonAnode = true` (lower duty = brighter). This doesn't overvolt the GPIOs
in normal operation since the Pico actively drives GP10–GP12 rather than the
LED imposing a voltage, but it does mean the common-cathode assumption in
earlier versions of this doc was backwards for the real hardware.

### Shared I²C bus
```
GP4 (SDA) and GP5 (SCL) → MCP23017 (0x22) and PIC bridge (0x04), in parallel
GP2 ← MCP INT A          GP3 ← MCP INT B
```
The board already has 4.7 kΩ pull-ups on SDA/SCL (`R73`/`R74`) — to `VCC`
(5 V), not 3V3. Route all four lines (SDA, SCL, INT A, INT B) through the
level shifter in §1a; don't add a second set of 3V3 pull-ups on top of the
board's existing ones.

---

## 4. Quick pinout (physical board, USB at top)

```
                ┌───────────  USB  ───────────┐
              ──┤ 1  GP0            VBUS  40 ├──  5V (USB)
              ──┤ 2  GP1            VSYS  39 ├──
         GND  ──┤ 3  GND            GND   38 ├──  GND
   MCP INT A  ──┤ 4  GP2          3V3_EN  37 ├──
   MCP INT B  ──┤ 5  GP3        3V3(OUT)  36 ├──  3.3V → peripherals
    I2C SDA   ──┤ 6  GP4       ADC_VREF   35 ├──
    I2C SCL   ──┤ 7  GP5            GP28  34 ├──
         GND  ──┤ 8  GND            AGND  33 ├──
              ──┤ 9  GP6            GP27  32 ├──
              ──┤ 10 GP7            GP26  31 ├──
              ──┤ 11 GP8             RUN  30 ├──
              ──┤ 12 GP9            GP22  29 ├──
         GND  ──┤ 13 GND            GND   28 ├──  GND
   LED Red    ──┤ 14 GP10           GP21  27 ├──  OLED RST
   LED Green  ──┤ 15 GP11           GP20  26 ├──  OLED DC
   LED Blue   ──┤ 16 GP12           GP19  25 ├──  OLED MOSI
              ──┤ 17 GP13           GP18  24 ├──  OLED SCLK
         GND  ──┤ 18 GND            GND   23 ├──  GND
   Encoder A  ──┤ 19 GP14           GP17  22 ├──  OLED CS
   Encoder B  ──┤ 20 GP15           GP16  21 ├──
                └─────────────────────────────┘
```
