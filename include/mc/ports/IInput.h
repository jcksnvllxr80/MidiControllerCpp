#pragma once
//
// IInput — the source of user actions. The original device read GPIO/MCP23017
// interrupts and an encoder; the Pi also accepted the same actions over the
// Flask short/long/dpad HTTP endpoints. We model the *semantic* events those
// produced, so the sim (scripted or keyboard) and a future GPIO adapter feed the
// Application the same way.
//
namespace mc {

struct InputEvent {
    enum class Type {
        None,
        FootswitchShort,  // short press of footswitch `button`  (Flask short/<button>)
        FootswitchLong,   // long press of footswitch `button`   (Flask long/<button>)
        EncoderCW,        // rotary turned clockwise             (Flask dpad/CW)
        EncoderCCW,       // rotary turned counter-clockwise      (Flask dpad/CCW)
        RotaryPress,      // rotary push button held `holdSeconds` (Flask dpad up/down + holds)
        Quit,             // stop the event loop
    };
    Type type = Type::None;
    int button = 0;            // for Footswitch*
    double holdSeconds = 0.0;  // for RotaryPress (classified by MenuTree::pressFor)
};

class IInput {
public:
    virtual ~IInput() = default;
    // Block/return the next event. Returns false when the source is exhausted
    // (scripted input ran out) -> the event loop ends.
    virtual bool poll(InputEvent& out) = 0;
};

}  // namespace mc
