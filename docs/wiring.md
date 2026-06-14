# Wiring — Pico 2 W (RP2350)

This is the physical build for the firmware as it ships. **The GPIO assignments
are defined in [`include/mc/adapters/mcu/Pins.h`](../include/mc/adapters/mcu/Pins.h)
— that header is the source of truth.** If you wire differently, edit `Pins.h` to
match (and watch the per-peripheral capability notes below: UART/I²C functions are
only available on certain GPIOs).

A visual version is in [`wiring.excalidraw`](wiring.excalidraw) — open it at
<https://excalidraw.com> (File → Open) or with the Excalidraw VS Code extension.
Regenerate it with `python3 tools/gen_wiring_excalidraw.py`.

All logic is **3.3 V**. Nothing here is 5 V tolerant — do not feed 5 V into any GPIO.

---

## 1. Pin map (everything the firmware uses)

| GPIO | Pico pin | Peripheral | Signal | External parts |
|-----:|---------:|------------|--------|----------------|
| GP0  | 1  | UART0 TX | **MIDI A** out | 220 Ω in series → DIN pin 5 |
| GP2  | 4  | GPIO in  | Footswitch 1 | SPST momentary → GND (internal pull-up) |
| GP3  | 5  | GPIO in  | Footswitch 2 | SPST momentary → GND |
| GP4  | 6  | GPIO in  | Footswitch 3 | SPST momentary → GND |
| GP5  | 7  | GPIO in  | Footswitch 4 | SPST momentary → GND |
| GP6  | 9  | GPIO in  | Footswitch 5 | SPST momentary → GND |
| GP7  | 10 | GPIO in  | Footswitch 6 | SPST momentary → GND |
| GP8  | 11 | UART1 TX | **MIDI B** out | 220 Ω in series → DIN pin 5 |
| GP10 | 14 | GPIO in  | Rotary encoder A | encoder pin A (internal pull-up) |
| GP11 | 15 | GPIO in  | Rotary encoder B | encoder pin B (internal pull-up) |
| GP12 | 16 | GPIO in  | Rotary push switch | switch → GND (internal pull-up) |
| GP13 | 17 | PWM      | Knob LED — Red | 220–470 Ω → LED R |
| GP14 | 19 | PWM      | Knob LED — Green | 220–470 Ω → LED G |
| GP15 | 20 | PWM      | Knob LED — Blue | 220–470 Ω → LED B |
| GP16 | 21 | GPIO out | Tempo 1 | → 1/4" tip (470 Ω series optional) |
| GP17 | 22 | GPIO out | Tempo 2 | → 1/4" tip |
| GP18 | 24 | GPIO out | Tempo 3 | → 1/4" tip |
| GP19 | 25 | GPIO out | Tempo 4 | → 1/4" tip |
| GP26 | 31 | I²C1 SDA | OLED SDA | 4.7 kΩ pull-up → 3V3 (if not on the module) |
| GP27 | 32 | I²C1 SCL | OLED SCL | 4.7 kΩ pull-up → 3V3 (if not on the module) |

Power/ground pins used: **3V3(OUT) = pin 36** (peripheral supply), **GND = pins
3, 8, 13, 18, 23, 28, 38** (use whichever is nearest each device), **VBUS = pin 40**
(5 V from USB, only if something needs 5 V — nothing here does).

Free for expansion: GP1, GP9, GP20, GP21, GP22, GP28.

---

## 2. Power

- The board is powered over **USB** (the same cable that carries the editor link).
- Peripherals run from **3V3(OUT), pin 36**. Tie all device grounds to the Pico GND.
- Rough current budget on 3V3(OUT): OLED ~10–20 mA, knob LED ~20–60 mA at full
  white, encoder/footswitches ~0. The CYW43 radio draws its own ~200 mA+ in bursts
  from the board's regulator. The onboard regulator has headroom for this set, but
  if you add high-current loads, regulate a separate 3.3 V rail from VSYS/VBUS
  rather than leaning on 3V3(OUT).
- Decoupling: a 100 nF ceramic across each device's VCC/GND, plus a 1–10 µF bulk
  cap near the OLED, keeps the I²C and the radio happy.

---

## 3. Per-device wiring

### OLED — SSD1306 128×64, I²C1 @ 400 kHz, address 0x3C
```
OLED VCC → 3V3 (pin 36)
OLED GND → GND
OLED SDA → GP26 (pin 31)
OLED SCL → GP27 (pin 32)
```
Most 0.96"/1.3" modules have pull-ups onboard; if yours doesn't, add 4.7 kΩ from
SDA and SCL to 3V3. The address is **0x3C** (`pins::OLED_I2C_ADDR`); a few modules
are 0x3D — change it in `Pins.h` if so. I²C1 SDA/SCL are only valid on specific
GPIOs (SDA: 2/6/10/14/18/22/**26**; SCL: 3/7/11/15/19/23/**27**) — keep them on a
valid pair if you move them.

### Footswitches ×6 — momentary SPST to ground
```
Each switch: one lug → GPIO (GP2..GP7), other lug → GND
```
No external resistors: the firmware enables internal pull-ups, so a pin reads HIGH
when open and is pulled LOW when stomped (active-low, 5 ms debounce). Button order
GP2→GP7 = buttons 1→6 in the config's `button_setup`.

### Rotary encoder + push switch (the menu knob)
```
Encoder A      → GP10 (pin 14)
Encoder B      → GP11 (pin 15)
Encoder common → GND
Push switch    → GP12 (pin 16), other side → GND
```
Internal pull-ups are on; A/B and the switch all idle HIGH and pull LOW. The
firmware assumes **4 quadrature steps per detent** (`kStepsPerDetent = 4`, typical
for mechanical encoders like EC11). If you get two steps per click or ghost steps,
that constant in `McuInput` is the thing to change.

### Knob RGB LED — PWM, common-cathode assumed
```
GP13 → 220–470 Ω → LED Red anode
GP14 → 220–470 Ω → LED Green anode
GP15 → 220–470 Ω → LED Blue anode
LED common cathode → GND
```
Each colour gets its own current-limit resistor. The firmware drives these PWM and
assumes a **common-cathode** LED (brighter = higher duty). If your knob LED is
**common-anode**, tie the common to 3V3 instead and construct `McuLed` with
`commonAnode = true` so the duty is inverted.

### MIDI DIN outputs ×2 (5-pin DIN, MIDI OUT)
Looking at the back of the panel-mount DIN-5 socket:
```
DIN pin 4 → 220 Ω → 3V3        (current-source leg)
DIN pin 5 → 220 Ω → UART TX    (MIDI A = GP0, MIDI B = GP8)
DIN pin 2 → GND / shield
DIN pins 1, 3 → no connection
```
31250 baud, 8N1, TX only — the firmware mirrors every message to **both** jacks
(`TeeMidiOut`). 220 Ω/220 Ω is the classic (5 V) MIDI-OUT network and works fine
driven from 3.3 V into a standard opto-isolated MIDI input; if you want to follow
the MIDI Association's 3.3 V reference exactly, use ~33 Ω on the pin-4 leg and
~10 Ω in series with TX. UART1 TX is only valid on GP4 or **GP8** (and UART0 TX on
GP0 or GP12) — keep MIDI on a valid TX pin if you move it.

### Tempo / clock outputs ×4 — 1/4" TS jacks
```
GP16 → Tempo 1 tip      sleeve → GND
GP17 → Tempo 2 tip      (a 470 Ω series resistor on each tip is cheap protection)
GP18 → Tempo 3 tip
GP19 → Tempo 4 tip
```
Each is a 3.3 V square wave, one full cycle per beat (50 % duty). If the gear you're
clocking expects a higher-voltage pulse or a contact-closure, buffer it (transistor
or opto) — the bare GPIO is a 3.3 V CMOS output.

---

## 4. Quick pinout (physical board, USB at top)

```
                ┌───────────  USB  ───────────┐
   MIDI A TX  ──┤ 1  GP0            VBUS  40 ├──  5V (USB)
              ──┤ 2  GP1            VSYS  39 ├──
         GND  ──┤ 3  GND            GND   38 ├──  GND
   Footsw 1  ──┤ 4  GP2          3V3_EN  37 ├──
   Footsw 2  ──┤ 5  GP3        3V3(OUT)  36 ├──  3.3V → peripherals
   Footsw 3  ──┤ 6  GP4       ADC_VREF   35 ├──
   Footsw 4  ──┤ 7  GP5            GP28  34 ├──
         GND  ──┤ 8  GND            AGND  33 ├──
   Footsw 5  ──┤ 9  GP6            GP27  32 ├──  OLED SCL
   Footsw 6  ──┤ 10 GP7            GP26  31 ├──  OLED SDA
   MIDI B TX  ──┤ 11 GP8             RUN  30 ├──
              ──┤ 12 GP9            GP22  29 ├──
         GND  ──┤ 13 GND            GND   28 ├──  GND
   Encoder A  ──┤ 14 GP10           GP21  27 ├──
   Encoder B  ──┤ 15 GP11           GP20  26 ├──
   Rotary SW  ──┤ 16 GP12           GP19  25 ├──  Tempo 4
   LED Red   ──┤ 17 GP13           GP18  24 ├──  Tempo 3
         GND  ──┤ 18 GND            GND   23 ├──  GND
   LED Green ──┤ 19 GP14           GP17  22 ├──  Tempo 2
   LED Blue  ──┤ 20 GP15           GP16  21 ├──  Tempo 1
                └─────────────────────────────┘
```
