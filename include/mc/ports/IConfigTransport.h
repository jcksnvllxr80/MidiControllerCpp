#pragma once
//
// IConfigTransport — the link to the controller/editor app. Originally this was
// WiFi + a Flask HTTP API (piMidiHttp); the plan replaces it with USB in Phase 3
// and a ported controller app in Phase 4. Defined now so the Application can own
// it; the basic sim provides a no-op implementation.
//
namespace mc {

class IConfigTransport {
public:
    virtual ~IConfigTransport() = default;
    virtual void begin() = 0;  // start listening (USB/HTTP)
    virtual void poll() = 0;   // service pending requests (non-blocking)
};

}  // namespace mc
