#pragma once
// USB-serial logger for the Pico firmware.
//
// Output goes to stdout, which pico_stdio_usb routes over the USB CDC ACM port.
// When no host is connected the SDK silently drops the bytes — no blocking, no
// stall. When the editor app or any serial monitor is connected the lines appear
// in real time alongside the JSON editor protocol (the app's JSON codec ignores
// non-JSON lines).
//
// Format: [L ms_stamp TAG] message
//   L        = E / W / I / D  (error / warn / info / debug)
//   ms_stamp = milliseconds since boot (7 digits)
//   TAG      = short subsystem label (e.g. "boot", "enc", "midi")
//
// Compile-time level gate via MC_LOG_LEVEL (default 3 = INFO):
//   0 = silent   1 = error   2 = +warn   3 = +info   4 = +debug
//
// IMPORTANT: Do NOT call LOG_* from IRQ context — printf is not IRQ-safe.
// Buffer anything needed in volatile state and log from main context.
#include <cstdarg>
#include <cstdint>
#include <cstdio>

// time_us_32() comes from the Pico SDK on-device. Host unit tests pull MCU adapter
// TUs in directly (e.g. test_mcu_config_store.cpp includes McuConfigStore.cpp, which
// includes this header) and build without the SDK on the include path — so only
// require pico/time.h when it's actually available, and fall back to a 0 stamp on
// the host. Keeps this logger usable from host tests instead of hard-failing.
#if __has_include("pico/time.h")
#include "pico/time.h"
#define MC_LOG_HAVE_PICO_TIME 1
#endif

#ifndef MC_LOG_LEVEL
#define MC_LOG_LEVEL 3
#endif

namespace mc::log {

// __attribute__((format)) lets the compiler check format strings at compile
// time the same way it checks printf calls — catches type mismatches early.
__attribute__((format(printf, 3, 4)))
inline void emit(char lvl, const char* tag, const char* fmt, ...) {
#ifdef MC_LOG_HAVE_PICO_TIME
    uint32_t ms = time_us_32() / 1000u;
#else
    uint32_t ms = 0;  // host build (no Pico SDK): no boot-time clock
#endif
    printf("[%c %7lu %s] ", lvl, static_cast<unsigned long>(ms), tag);
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    putchar('\n');
}

}  // namespace mc::log

#if MC_LOG_LEVEL >= 1
#define LOG_E(tag, ...) mc::log::emit('E', tag, __VA_ARGS__)
#else
#define LOG_E(tag, ...) ((void)0)
#endif

#if MC_LOG_LEVEL >= 2
#define LOG_W(tag, ...) mc::log::emit('W', tag, __VA_ARGS__)
#else
#define LOG_W(tag, ...) ((void)0)
#endif

#if MC_LOG_LEVEL >= 3
#define LOG_I(tag, ...) mc::log::emit('I', tag, __VA_ARGS__)
#else
#define LOG_I(tag, ...) ((void)0)
#endif

#if MC_LOG_LEVEL >= 4
#define LOG_D(tag, ...) mc::log::emit('D', tag, __VA_ARGS__)
#else
#define LOG_D(tag, ...) ((void)0)
#endif
