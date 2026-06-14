#!/usr/bin/env python3
"""Generate docs/wiring.excalidraw — a visual wiring diagram for the Pico 2 W build.

The pin assignments mirror include/mc/adapters/mcu/Pins.h. Re-run after changing
pins:  python3 tools/gen_wiring_excalidraw.py
Open the result at https://excalidraw.com (File -> Open) or the VS Code extension.
"""
import json
import os

PITCH = 34          # vertical px between adjacent pins
TOP = 170           # y of the first (top) pin row
BOARD_X, BOARD_W = 540, 240
BOARD_L, BOARD_R = BOARD_X, BOARD_X + BOARD_W
LBOX_X, LBOX_W = 110, 310           # left peripheral column
RBOX_X, RBOX_W = 860, 330           # right peripheral column

# (physical pin, label, used?) — left column top->bottom is pins 1..20
LEFT = [
    (1, "GP0", True), (2, "GP1", False), (3, "GND", False), (4, "GP2", True),
    (5, "GP3", True), (6, "GP4", True), (7, "GP5", True), (8, "GND", False),
    (9, "GP6", True), (10, "GP7", True), (11, "GP8", True), (12, "GP9", False),
    (13, "GND", False), (14, "GP10", True), (15, "GP11", True), (16, "GP12", True),
    (17, "GP13", True), (18, "GND", False), (19, "GP14", True), (20, "GP15", True),
]
# right column top->bottom is pins 40..21
RIGHT = [
    (40, "VBUS", True), (39, "VSYS", False), (38, "GND", False), (37, "3V3_EN", False),
    (36, "3V3(OUT)", True), (35, "ADC_VREF", False), (34, "GP28", False), (33, "AGND", False),
    (32, "GP27", True), (31, "GP26", True), (30, "RUN", False), (29, "GP22", False),
    (28, "GND", False), (27, "GP21", False), (26, "GP20", False), (25, "GP19", True),
    (24, "GP18", True), (23, "GND", False), (22, "GP17", True), (21, "GP16", True),
]
# (title text, side, [row indices], fill)
PERIPHERALS = [
    ("MIDI A out\nGP0 -> 220R -> DIN-5 pin 5", "L", [0], "#ffd8a8"),
    ("6x Footswitch\nGP2..GP7 -> GND\n(internal pull-ups, active-low)", "L", [3, 4, 5, 6, 8, 9], "#a5d8ff"),
    ("MIDI B out\nGP8 -> 220R -> DIN-5 pin 5", "L", [10], "#ffd8a8"),
    ("Rotary encoder + push\nA=GP10  B=GP11  SW=GP12\ncommon -> GND", "L", [13, 14, 15], "#b2f2bb"),
    ("Knob RGB LED (PWM)\nR=GP13 G=GP14 B=GP15\n220-470R each, common -> GND", "L", [16, 18, 19], "#eebefa"),
    ("Power\nVBUS = 5V (USB)\n3V3(OUT) -> all peripherals", "R", [0, 4], "#ffec99"),
    ("OLED SSD1306  I2C1 @ 0x3C\nSDA=GP26  SCL=GP27\nVCC=3V3  GND  (400 kHz)", "R", [8, 9], "#a5d8ff"),
    ("4x Tempo out (1/4\")\nGP16 GP17 GP18 GP19\n3.3V square wave, sleeve -> GND", "R", [15, 16, 18, 19], "#b2f2bb"),
]

elements = []
_id = [0]


def _base(t, x, y, w, h, stroke="#1e1e1e", bg="transparent", fill="solid",
          sw=2, rough=1, roundness=None):
    _id[0] += 1
    i = _id[0]
    return dict(id=f"e{i}", type=t, x=x, y=y, width=w, height=h, angle=0,
                strokeColor=stroke, backgroundColor=bg, fillStyle=fill,
                strokeWidth=sw, strokeStyle="solid", roughness=rough, opacity=100,
                groupIds=[], frameId=None, roundness=roundness, seed=i * 1000 + 7,
                version=1, versionNonce=i * 131 + 3, isDeleted=False,
                boundElements=[], updated=1, link=None, locked=False)


def rect(x, y, w, h, bg, stroke="#1e1e1e", sw=2):
    elements.append(_base("rectangle", x, y, w, h, stroke=stroke, bg=bg, sw=sw,
                          roundness={"type": 3}))


def text(x, y, s, size=14, color="#1e1e1e", align="left", font=3, w=None):
    longest = max(s.split("\n"), key=len)
    if w is None:
        w = max(12, int(len(longest) * size * 0.62))
    h = int((s.count("\n") + 1) * size * 1.25)
    e = _base("text", x, y, w, h, stroke=color)
    e.update(text=s, fontSize=size, fontFamily=font, textAlign=align,
             verticalAlign="top", baseline=int(size * 0.9), containerId=None,
             originalText=s, lineHeight=1.25, autoResize=True)
    elements.append(e)


def line(x1, y1, x2, y2, color="#1e1e1e", sw=1.5):
    x, y = min(x1, x2), min(y1, y2)
    e = _base("line", x, y, abs(x2 - x1), abs(y2 - y1), stroke=color, sw=sw, rough=0)
    e.update(points=[[x1 - x, y1 - y], [x2 - x, y2 - y]], lastCommittedPoint=None,
             startBinding=None, endBinding=None, startArrowhead=None, endArrowhead=None)
    elements.append(e)


def dot(x, y, color="#e03131"):
    elements.append(_base("ellipse", x - 4, y - 4, 8, 8, stroke=color, bg=color))


def row_y(i):
    return TOP + i * PITCH


# --- titles -----------------------------------------------------------------
text(LBOX_X, 86, "MIDI Controller - Pico 2 W (RP2350) wiring", size=28, font=2)
text(LBOX_X, 126, "GPIO assignments from include/mc/adapters/mcu/Pins.h  -  all logic 3.3V",
     size=14, color="#495057", font=2)

# --- board ------------------------------------------------------------------
rect(BOARD_L, TOP - 22, BOARD_W, 20 * PITCH + 8, "#f1f3f5")
text(BOARD_L + 78, TOP - 44, "USB", size=13, color="#868e96", font=2)
text(BOARD_L + 60, TOP - 20, "Pico 2 W", size=13, color="#adb5bd", font=2)

# pin labels (used = dark, unused = gray) + connection dots on used pins
for i, (pin, name, used) in enumerate(LEFT):
    c = "#1e1e1e" if used else "#adb5bd"
    text(BOARD_L + 10, row_y(i) - 2, f"{pin:>2}  {name}", size=13, color=c)
    if used:
        dot(BOARD_L, row_y(i) + 7)
for i, (pin, name, used) in enumerate(RIGHT):
    c = "#1e1e1e" if used else "#adb5bd"
    text(BOARD_R - 160, row_y(i) - 2, f"{name}  {pin:>2}", size=13, color=c,
         align="right", w=150)
    if used:
        dot(BOARD_R, row_y(i) + 7)

# --- peripheral boxes + connectors -----------------------------------------
for title, side, rows, fill in PERIPHERALS:
    ys = [row_y(r) + 7 for r in rows]
    top, bot = min(ys), max(ys)
    bx = LBOX_X if side == "L" else RBOX_X
    bw = LBOX_W if side == "L" else RBOX_W
    by = top - 16
    bh = max(bot - top + 32, 58)
    rect(bx, by, bw, bh, fill)
    text(bx + 12, by + 10, title, size=13, w=bw - 24)
    edge = (bx + bw) if side == "L" else bx        # box edge facing the board
    board_edge = BOARD_L if side == "L" else BOARD_R
    for yy in ys:
        line(edge, yy, board_edge, yy, color="#343a40")

# --- footer note ------------------------------------------------------------
text(LBOX_X, row_y(20) + 24,
     "Notes:  Pins are firmware-defined - edit Pins.h to match your wiring.\n"
     "MIDI DIN-5: pin 4 -> 220R -> 3V3, pin 5 -> 220R -> UART TX, pin 2 -> GND.\n"
     "I2C1 only on SDA GP26 / SCL GP27 here; UART TX only on GP0 (uart0) and GP8 (uart1).\n"
     "Red dots = pins the firmware drives.  See docs/wiring.md for the full build.",
     size=13, color="#495057", font=2)

doc = {"type": "excalidraw", "version": 2, "source": "tools/gen_wiring_excalidraw.py",
       "elements": elements,
       "appState": {"gridSize": None, "viewBackgroundColor": "#ffffff"},
       "files": {}}

out = os.path.join(os.path.dirname(__file__), "..", "docs", "wiring.excalidraw")
with open(os.path.normpath(out), "w", encoding="utf-8") as f:
    json.dump(doc, f, indent=2)
print(f"wrote {os.path.normpath(out)}  ({len(elements)} elements)")
