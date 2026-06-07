#pragma once
//
// IClock — monotonic time source, in seconds (mirrors Python's time.time()).
// Injected so the button/encoder timing logic is testable with a fake clock.
//
namespace mc {

class IClock {
public:
    virtual ~IClock() = default;
    virtual double now() const = 0;  // seconds
};

}  // namespace mc
