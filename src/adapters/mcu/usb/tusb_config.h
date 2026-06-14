#pragma once
//
// TinyUSB device config for the raw-USB editor link: a single VENDOR interface
// (bulk IN/OUT) that Windows auto-binds to WinUSB via the MS-OS-2.0 descriptors
// in usb_descriptors.c, so the host app's `nusb` transport can claim it.
//
// Compiled only when the Pico build is configured with -DMC_ENABLE_USB_EDITOR=ON.
// In that build stdio moves to UART (CMakeLists), so USB carries only this link.
// CFG_TUSB_MCU / CFG_TUSB_OS come from the Pico SDK's tinyusb integration.
//
#ifdef __cplusplus
extern "C" {
#endif

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS OPT_OS_PICO
#endif

#define CFG_TUSB_RHPORT0_MODE (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)

#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif
#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN __attribute__((aligned(4)))
#endif

#define CFG_TUD_ENABLED 1

// One vendor interface; everything else off.
#define CFG_TUD_CDC 0
#define CFG_TUD_MSC 0
#define CFG_TUD_HID 0
#define CFG_TUD_MIDI 0
#define CFG_TUD_VENDOR 1

// Buffers sized for the JSON line protocol (a large get_pedal reply streams out
// in chunks; see UsbConfigTransport).
#define CFG_TUD_VENDOR_RX_BUFSIZE 256
#define CFG_TUD_VENDOR_TX_BUFSIZE 256

#ifdef __cplusplus
}
#endif
