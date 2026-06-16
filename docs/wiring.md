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
Encoder A      → GP14 (pin 19)
Encoder B      → GP15 (pin 20)
Encoder common → GND
```
Internal pull-ups; A/B idle high and pull low. 4 quadrature steps per detent
(`kStepsPerDetent = 4`). **The push button is not here** — it's MCP bit B7, read over
I²C with the footswitches.

### Knob RGB LED — PWM, common-cathode assumed
```
GP10 → 220–470 Ω → LED Red anode
GP11 → 220–470 Ω → LED Green anode
GP12 → 220–470 Ω → LED Blue anode
LED common cathode → GND
```
Each colour gets its own resistor. The firmware drives these PWM and assumes a
common-cathode LED (brighter = higher duty); for common-anode, tie the common to 3V3
and construct `McuLed` with `commonAnode = true`.

### Shared I²C bus
```
GP4 (SDA) and GP5 (SCL) → MCP23017 (0x22) and PIC bridge (0x04), in parallel
GP2 ← MCP INT A          GP3 ← MCP INT B
```
Add 4.7 kΩ pull-ups on SDA/SCL to 3V3 if the board doesn't already have them.

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
