#pragma once
//
// McuInput — IInput from real hardware: 6 footswitches (debounced, classified
// short/long via the domain ButtonSM), a quadrature rotary encoder (CW/CCW per
// detent), and the rotary push button (hold time -> RotaryPress seconds).
//
// poll() is non-blocking: it services the pins and returns the next queued event,
// or false if none. The MCU main loop calls it in a tight loop (it does NOT use
// Application::run(), whose "false ends the loop" contract is for scripted input).
//
#include <array>
#include <cstdint>
#include <vector>

#include "mc/domain/ButtonSM.h"
#include "mc/ports/IClock.h"
#include "mc/ports/IInput.h"

namespace mc::mcu {

class McuInput : public IInput {
public:
    McuInput(const IClock& clock, const uint8_t* footswitches, int footswitchCount,
             uint8_t encoderA, uint8_t encoderB, uint8_t rotaryPb);
    void begin();  // configure GPIOs
    bool poll(InputEvent& out) override;

private:
    void service();
    void push(const InputEvent& e);
    bool accept(int idx, bool level, double nowS);  // simple time debounce

    static constexpr double kDebounce = 0.005;     // 5 ms
    static constexpr int kStepsPerDetent = 4;      // typical mechanical encoder
    static constexpr size_t kRing = 32;

    const IClock& clock_;
    const uint8_t* fsw_;
    int fswCount_;
    uint8_t encA_, encB_, rotaryPb_;

    std::vector<ButtonSM> buttons_;  // one per footswitch

    // debounce: index 0..fswCount-1 = footswitches, fswCount = rotary PB
    std::array<bool, 8> level_{};
    std::array<double, 8> changedAt_{};

    uint8_t encPrev_ = 0;
    int encAccum_ = 0;

    bool rotaryDown_ = false;
    double rotaryStart_ = 0.0;

    std::array<InputEvent, kRing> ring_{};
    size_t head_ = 0, tail_ = 0;
};

}  // namespace mc::mcu
