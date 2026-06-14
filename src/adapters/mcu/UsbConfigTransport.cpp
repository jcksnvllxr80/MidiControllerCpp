// Built only with -DMC_ENABLE_USB_EDITOR=ON (see UsbConfigTransport.h). Needs
// the vendor tusb_config.h + usb_descriptors.c and the tinyusb_device library.
#include "mc/adapters/mcu/UsbConfigTransport.h"

#include <algorithm>

#include "tusb.h"

namespace mc::mcu {

void UsbConfigTransport::begin() { tusb_init(); }

void UsbConfigTransport::poll() {
    tud_task();
    while (tud_vendor_available()) {
        char buf[64];
        uint32_t n = tud_vendor_read(buf, sizeof buf);
        for (uint32_t i = 0; i < n; ++i) {
            char c = buf[i];
            if (c == '\n') {
                dispatch();
            } else if (c == '\r') {
                // ignore CR
            } else if (rxLine_.size() < kMaxLine) {
                rxLine_.push_back(c);
            }
        }
    }
}

void UsbConfigTransport::dispatch() {
    std::string resp = proto_.handleLine(rxLine_);
    rxLine_.clear();
    if (resp.empty()) return;

    // Stream the reply out the bulk IN endpoint, pumping tud_task() so the TX
    // FIFO drains as we go — handles replies far larger than CFG_TUD_VENDOR_TX
    // (e.g. a ~30 KB get_pedal payload). Bails if the host disconnects.
    size_t off = 0;
    while (off < resp.size()) {
        if (!tud_mounted()) break;
        uint32_t space = tud_vendor_write_available();
        if (space == 0) {
            tud_task();  // let the stack push buffered data, freeing FIFO space
            continue;
        }
        uint32_t chunk = static_cast<uint32_t>(std::min<size_t>(space, resp.size() - off));
        off += tud_vendor_write(resp.data() + off, chunk);
        tud_vendor_write_flush();
        tud_task();
    }
}

}  // namespace mc::mcu
