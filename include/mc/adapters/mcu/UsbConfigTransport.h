#pragma once
//
// UsbConfigTransport — EditorProtocol over a raw TinyUSB VENDOR interface (bulk
// IN/OUT), the firmware side of the host app's `nusb` raw-USB transport. Same
// newline-JSON protocol as StdioConfigTransport, but over our own vendor
// endpoints instead of the SDK's stdio CDC.
//
// Built only with -DMC_ENABLE_USB_EDITOR=ON (needs tusb_config.h +
// usb_descriptors.c in src/adapters/mcu/usb/ and the tinyusb_device library).
// In that build stdio moves to UART so USB carries only this link.
//
#include <cstddef>
#include <string>

#include "mc/app/EditorProtocol.h"
#include "mc/ports/IConfigTransport.h"

namespace mc::mcu {

class UsbConfigTransport : public IConfigTransport {
public:
    explicit UsbConfigTransport(EditorProtocol& proto) : proto_(proto) {}

    void begin() override;  // tusb_init()
    void poll() override;   // tud_task() + drain the vendor RX, dispatch on newline

private:
    void dispatch();

    EditorProtocol& proto_;
    std::string rxLine_;
    static constexpr size_t kMaxLine = 128 * 1024;  // big enough for a write_pedal payload
};

}  // namespace mc::mcu
