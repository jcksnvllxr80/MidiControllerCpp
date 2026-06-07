#pragma once
#include "mc/ports/IClock.h"

namespace mc::test {

class FakeClock : public IClock {
public:
    double now() const override { return t_; }
    void set(double t) { t_ = t; }
    void advance(double dt) { t_ += dt; }

private:
    double t_ = 0.0;
};

}  // namespace mc::test
