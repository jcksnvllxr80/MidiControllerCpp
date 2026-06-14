#pragma once
//
// McuSystemControl — ISystemControl on the RP2350. A reboot request is recorded,
// then carried out from poll() after a short grace period so the protocol ack has
// time to flush over USB/TCP before the device resets and re-enumerates.
//
#include <cstdint>

#include "mc/ports/ISystemControl.h"

namespace mc::mcu {

class McuSystemControl : public ISystemControl {
public:
    void rebootToBootloader() override;  // schedule: drop into USB BOOTSEL on next poll()
    void reboot() override;              // schedule: normal reboot into the application
    void poll();                         // perform a scheduled reset once the ack has flushed

private:
    enum class Pending { None, Bootloader, App };
    Pending pending_ = Pending::None;
    uint32_t requestedAtMs_ = 0;
};

}  // namespace mc::mcu
