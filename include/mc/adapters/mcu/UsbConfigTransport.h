#pragma once
//
// UsbConfigTransport — IConfigTransport over USB CDC (TinyUSB). This is the
// firmware side of the link to the external editor app (a separate project).
//
// Requires TinyUSB to be configured in the build: a tusb_config.h and USB
// descriptors (usb_descriptors.c) — board-specific boilerplate not shipped here.
// Until that's in place, use NoopConfigTransport. The command/response protocol
// itself belongs to the editor project; poll() just services USB and buffers
// incoming bytes for that handler.
//
#include <string>

#include "mc/ports/IConfigTransport.h"

namespace mc::mcu {

class UsbConfigTransport : public IConfigTransport {
public:
    void begin() override;  // tud_init
    void poll() override;   // tud_task + drain CDC RX

private:
    std::string rxLine_;
    // TODO(phase3): hand complete lines to the editor protocol handler.
};

}  // namespace mc::mcu
