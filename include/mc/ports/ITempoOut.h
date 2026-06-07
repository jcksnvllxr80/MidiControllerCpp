#pragma once
//
// ITempoOut — the tempo outputs (4x 1/4" tempo jacks + tap tempo). These were
// the Arduino's job before; the plan promotes them to a first-class port now so
// the firmware owns tempo. Phase-1 sim just logs; Phase-3 mcu pulses real jacks.
//
namespace mc {

class ITempoOut {
public:
    virtual ~ITempoOut() = default;
    virtual void setBpm(double bpm) = 0;
    virtual void tap() = 0;
};

}  // namespace mc
