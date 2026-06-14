#include "mc/adapters/mcu/McuSystemControl.h"

#include "hardware/watchdog.h"
#include "pico/bootrom.h"  // reset_usb_boot
#include "pico/time.h"

namespace mc::mcu {

namespace {
constexpr uint32_t kFlushGraceMs = 100;  // let the editor ack flush before we vanish
uint32_t nowMs() { return to_ms_since_boot(get_absolute_time()); }
}  // namespace

void McuSystemControl::rebootToBootloader() {
    pending_ = Pending::Bootloader;
    requestedAtMs_ = nowMs();
}

void McuSystemControl::reboot() {
    pending_ = Pending::App;
    requestedAtMs_ = nowMs();
}

void McuSystemControl::poll() {
    if (pending_ == Pending::None) return;
    if (nowMs() - requestedAtMs_ < kFlushGraceMs) return;  // give the ack time to flush
    if (pending_ == Pending::Bootloader)
        reset_usb_boot(0, 0);   // -> BOOTSEL / USB mass storage (noreturn)
    watchdog_reboot(0, 0, 0);   // -> normal reboot into the application (noreturn)
}

}  // namespace mc::mcu
