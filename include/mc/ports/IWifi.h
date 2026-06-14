#pragma once
//
// IWifi — the WiFi control port. The protocol/app layer sets credentials and
// toggles WiFi through this; the mcu adapter (WifiManager) drives the CYW43 +
// lwIP behind it. Hardware-free so EditorProtocol stays host-testable.
//
#include <string>

namespace mc {

struct WifiStatus {
    bool enabled = false;
    bool connected = false;
    std::string ssid;
    std::string ip;  // "" until connected
};

class IWifi {
public:
    virtual ~IWifi() = default;
    // Store credentials (persisted) and (re)attempt a connection.
    virtual void setCredentials(const std::string& ssid, const std::string& password) = 0;
    // Enable/disable WiFi (persisted); disabling drops the connection.
    virtual void setEnabled(bool on) = 0;
    virtual WifiStatus status() const = 0;
};

}  // namespace mc
