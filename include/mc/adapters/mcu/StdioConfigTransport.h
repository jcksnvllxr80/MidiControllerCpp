#pragma once
//
// StdioConfigTransport — runs EditorProtocol over the SDK's stdio USB CDC (the
// port the editor app connects to). No custom USB descriptors needed: the app's
// codec skips non-JSON lines, so protocol replies and debug printf coexist on
// the one CDC. poll() drains stdin, dispatches a line on newline, writes the
// reply to stdout.
//
#include <cstdio>
#include <string>

#include "pico/stdlib.h"  // getchar_timeout_us, PICO_ERROR_TIMEOUT, stdio_flush

#include "mc/app/EditorProtocol.h"
#include "mc/ports/IConfigTransport.h"

namespace mc::mcu {

class StdioConfigTransport : public IConfigTransport {
public:
    explicit StdioConfigTransport(EditorProtocol& proto) : proto_(proto) {}

    void begin() override {}  // stdio already initialised in main()

    void poll() override {
        int c;
        while ((c = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
            if (c == '\n') {
                dispatch();
            } else if (c == '\r') {
                // ignore CR
            } else if (line_.size() < kMaxLine) {
                line_.push_back(static_cast<char>(c));
            }
        }
    }

private:
    void dispatch() {
        std::string resp = proto_.handleLine(line_);
        line_.clear();
        if (!resp.empty()) {
            // Write the whole reply, then fflush() to force newlib's (fully-
            // buffered) stdout out in big chunks — efficient even for ~30 KB
            // get_pedal payloads, and unbuffered enough that small replies leave.
            std::fwrite(resp.data(), 1, resp.size(), stdout);
            std::fflush(stdout);
            stdio_flush();
        }
    }

    EditorProtocol& proto_;
    std::string line_;
    static constexpr size_t kMaxLine = 128 * 1024;  // big enough for a write_pedal payload
};

}  // namespace mc::mcu
