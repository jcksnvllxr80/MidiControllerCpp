#pragma once
//
// NoopConfigTransport — the firmware runs fine without a host link. Use this for
// first bring-up; swap in UsbConfigTransport once TinyUSB descriptors are set up.
//
#include "mc/ports/IConfigTransport.h"

namespace mc::mcu {

class NoopConfigTransport : public IConfigTransport {
public:
    void begin() override {}
    void poll() override {}
};

}  // namespace mc::mcu
