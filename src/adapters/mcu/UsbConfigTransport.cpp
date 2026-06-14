// NOT in the default CMake build — needs TinyUSB configured (tusb_config.h +
// usb_descriptors.c). Add this file and `tinyusb_device tinyusb_board` to the
// target once that boilerplate is in place.
#include "mc/adapters/mcu/UsbConfigTransport.h"

#include "tusb.h"

namespace mc::mcu {

void UsbConfigTransport::begin() { tud_init(0); }

void UsbConfigTransport::poll() {
    tud_task();
    if (tud_cdc_available()) {
        char buf[64];
        uint32_t n = tud_cdc_read(buf, sizeof buf);
        for (uint32_t i = 0; i < n; ++i) {
            char c = buf[i];
            if (c == '\n') {
                // TODO(phase3): hand rxLine_ to the editor protocol handler.
                rxLine_.clear();
            } else {
                rxLine_.push_back(c);
            }
        }
    }
}

}  // namespace mc::mcu
