#pragma once
//
// Pins — one place for every GPIO assignment on the Pico 2 W (RP2350).
// EDIT THESE to match your wiring. GP numbers are RP2350 GPIOs.
//
#include <cstdint>

namespace mc::mcu::pins {

// 6 footswitches (active-low, internal pull-ups). Order = button numbers 1..6
// as used by ControllerState.button_setup. (Button "15" in the Python was the
// rotary push button; here that's ROTARY_PB below.)
inline constexpr uint8_t FOOTSWITCH[6] = {2, 3, 4, 5, 6, 7};

// Rotary encoder + its push button.
inline constexpr uint8_t ENCODER_A = 10;
inline constexpr uint8_t ENCODER_B = 11;
inline constexpr uint8_t ROTARY_PB = 12;  // active-low

// RGB LED in the knob (PWM). Common-cathode assumed; flip in McuLed if anode.
inline constexpr uint8_t LED_R = 13;
inline constexpr uint8_t LED_G = 14;
inline constexpr uint8_t LED_B = 15;

// Two MIDI DIN outputs (UART TX, 31250 baud). uart0 / uart1.
inline constexpr uint8_t MIDI_A_TX = 0;  // uart0 TX
inline constexpr uint8_t MIDI_B_TX = 8;  // uart1 TX

// Four 1/4" tempo pulse outputs.
inline constexpr uint8_t TEMPO[4] = {16, 17, 18, 19};

// SSD1306 OLED over I2C (i2c1 here so it doesn't clash with MIDI_A on uart0).
inline constexpr uint8_t OLED_SDA = 26;
inline constexpr uint8_t OLED_SCL = 27;
inline constexpr uint8_t OLED_I2C_ADDR = 0x3C;

}  // namespace mc::mcu::pins
