#pragma once
//
// EditorProtocol — the firmware side of the editor app's wire protocol
// (MidiControllerControllerApp). Newline-delimited JSON, one object per line,
// with an `id` echoed back for correlation:
//
//   <- {"id":N,"op":"identify"}
//   -> {"id":N,"ok":true,"data":{"name":...,"firmware":...,"protocol_version":N}}
//
// Hardware-free: a transport feeds it received lines and writes back the result.
// Config ops go through IConfigStore; live control (dpad/short) drives Application.
//
#include <string>

#include "mc/app/Application.h"
#include "mc/ports/IConfigStore.h"

namespace mc {

class EditorProtocol {
public:
    EditorProtocol(IConfigStore& store, Application& app, std::string deviceName = "MidiController",
                   std::string firmware = "cpp-0.1", int protocolVersion = 1);

    // Process one received line. Returns the response line (with trailing '\n'),
    // or "" for a blank / non-JSON "noise" line (nothing to send — the app's
    // codec skips those by design).
    std::string handleLine(const std::string& line);

private:
    IConfigStore& store_;
    Application& app_;
    std::string name_;
    std::string firmware_;
    int version_;
};

}  // namespace mc
