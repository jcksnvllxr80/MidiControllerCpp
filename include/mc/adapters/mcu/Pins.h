#pragma once
//
// Pins — every GPIO assignment on the Pico 2 W (RP2350) driving board.
//
// This board is a DROP-IN REPLACEMENT for the Raspberry Pi 4 that previously
// drove the (unchanged) MidiController PCB. The old Pi 40-pin ribbon now lands on
// an adapter that routes each net to the Pico GP below. Nothing on the PCB changed,
// so the firmware must speak the SAME buses the Pi did:
//
//   * MCP23017 @ I2C 0x22 — 5 footswitches + rotary push button (inputs, read over
//     I2C; two interrupt lines signal a change). The Pico never sees switch GPIOs.
//   * PIC18F26K80 @ I2C 0x04 — MIDI bridge. We write raw MIDI bytes over I2C and the
//     PIC fans them out to all 6 jacks (2x DIN-5 + 4x TRS-MIDI). No native UART MIDI.
//   * SSD1306 OLED over SPI (write-only: MOSI/SCLK + CS/DC/RST). No MISO.
//   * Rotary encoder A/B — native GPIO with edge interrupts + pull-ups.
//   * RGB knob LED — 3x PWM (common drive; duty inverted in McuLed if needed).
//
// Both I2C devices share ONE bus, exactly as on the Pi (single bus, 0x22 + 0x04).
//
#include <cstdint>

namespace mc::mcu::pins {

// ---- Shared I2C bus (i2c0): MCP23017 expander + PIC MIDI bridge ----
inline constexpr uint8_t I2C_SDA = 4;   // i2c0 SDA
inline constexpr uint8_t I2C_SCL = 5;   // i2c0 SCL

inline constexpr uint8_t MCP23017_ADDR = 0x22;  // footswitches + rotary push button
inline constexpr uint8_t MIDI_PIC_ADDR = 0x04;  // raw-MIDI -> 6-jack fan-out bridge

// MCP23017 interrupt lines into the Pico (active per the expander's IOCON config).
// Bank A covers footswitches on port A; bank B covers port B + the push button.
inline constexpr uint8_t MCP_INT_A = 2;  // GPIO in, IRQ
inline constexpr uint8_t MCP_INT_B = 3;  // GPIO in, IRQ

// Expander bit indices (NOT Pico GPIOs) — fixed by the PCB / the Python firmware.
// Footswitches 1..5 in button order; the selector is the rotary push button.
inline constexpr uint8_t FOOTSWITCH_BITS[5] = {0, 1, 2, 8, 10};  // A0,A1,A2,B0,B2
inline constexpr uint8_t SELECTOR_BIT = 15;                      // B7 (rotary push)

// ---- SSD1306 OLED over SPI (spi0), write-only ----
inline constexpr uint8_t OLED_SCLK = 18;  // spi0 SCK
inline constexpr uint8_t OLED_MOSI = 19;  // spi0 TX
inline constexpr uint8_t OLED_CS   = 17;  // chip select  (driven as plain GPIO)
inline constexpr uint8_t OLED_DC   = 20;  // data/command
inline constexpr uint8_t OLED_RST  = 21;  // reset

// ---- Rotary encoder (native GPIO, pull-ups, edge-sampled) ----
// Matches the ORIGINAL working Pi firmware: ENCODE_A = GPIO24 -> GP14,
// ENCODE_B = GPIO23 -> GP15 (RotaryEncoder.py / midi_controller.py). NOTE: the
// PCB net names "RotEncA/B" are labeled opposite to what that firmware calls
// A/B — the running code is authoritative, so A=GP14, B=GP15.
inline constexpr uint8_t ENCODER_A = 14;
inline constexpr uint8_t ENCODER_B = 15;
// NOTE: the rotary PUSH BUTTON is NOT a Pico GPIO — it is an MCP23017 input
// (bit 15 in the Python firmware) read over I2C like the footswitches.

// ---- RGB knob LED (PWM). Red/Green share PWM slice 5 (chans A/B); Blue on slice 6. ----
inline constexpr uint8_t LED_R = 10;
inline constexpr uint8_t LED_G = 11;
inline constexpr uint8_t LED_B = 12;

}  // namespace mc::mcu::pins
