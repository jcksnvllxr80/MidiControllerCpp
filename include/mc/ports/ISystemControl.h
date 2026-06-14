#pragma once
//
// ISystemControl — device reset/power control, behind a port so EditorProtocol
// stays hardware-free and host-testable. The MCU adapter resets into the USB
// BOOTSEL bootloader (to accept a .uf2 with no front button) or reboots into the
// application; the sim/tests use a fake.
//
// Implementations SCHEDULE the reset rather than resetting in-call, so the
// protocol's `{"ok":true}` ack can flush back to the editor before the device
// disappears (the actual reset happens a few ms later, e.g. from a poll()).
//
namespace mc {

class ISystemControl {
public:
    virtual ~ISystemControl() = default;
    virtual void rebootToBootloader() = 0;  // -> USB BOOTSEL / mass storage (UF2 mode)
    virtual void reboot() = 0;              // -> normal reboot into the application
};

}  // namespace mc
