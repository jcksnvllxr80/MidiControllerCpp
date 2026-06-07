#pragma once
//
// IConfigTransport — the firmware's link to an external editor app (a separate
// project). Originally WiFi + a Flask HTTP API (piMidiHttp); on the microcontroller
// this becomes USB (Phase 3). Defined now so the Application can own it; the basic
// sim provides a no-op implementation.
//
namespace mc {

class IConfigTransport {
public:
    virtual ~IConfigTransport() = default;
    virtual void begin() = 0;  // start listening (USB/HTTP)
    virtual void poll() = 0;   // service pending requests (non-blocking)
};

}  // namespace mc
