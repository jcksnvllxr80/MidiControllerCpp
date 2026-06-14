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
#include "mc/ports/ISystemControl.h"
#include "mc/ports/IWifi.h"

namespace mc {

class EditorProtocol {
public:
    // `wifi`/`sys` are optional: when null, their ops report "unsupported".
    EditorProtocol(IConfigStore& store, Application& app, IWifi* wifi = nullptr,
                   ISystemControl* sys = nullptr, std::string deviceName = "MidiController",
                   std::string firmware = "cpp-0.1", int protocolVersion = 1);

    // A stable per-unit id (e.g. the RP2350 board id) surfaced in `identify`, so the
    // editor can dedupe the same physical device seen over both USB and WiFi.
    void setDeviceId(std::string id) { deviceId_ = std::move(id); }

    // Process one received line. Returns the response line (with trailing '\n'),
    // or "" for a blank / non-JSON "noise" line (nothing to send — the app's
    // codec skips those by design).
    std::string handleLine(const std::string& line);

private:
    IConfigStore& store_;
    Application& app_;
    IWifi* wifi_;
    ISystemControl* sys_;
    std::string name_;
    std::string firmware_;
    int version_;
    std::string deviceId_;
};

}  // namespace mc
