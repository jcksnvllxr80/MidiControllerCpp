#pragma once
//
// WifiManager — CYW43 STA + lwIP TCP server (running the same EditorProtocol the
// USB link uses) + mDNS advertisement. Implements IWifi so the protocol can set
// credentials / toggle WiFi. Credentials persist via IConfigStore ("wifi.json").
//
// lwIP runs in POLL mode, so all TCP callbacks fire inside poll() on the main
// loop — no concurrency with the USB transport or the Application.
//
// NOTE: compile-verified only; not exercised on a real network here.
//
#include <cstdint>
#include <functional>
#include <string>

#include "mc/ports/IConfigStore.h"
#include "mc/ports/IWifi.h"

struct tcp_pcb;  // lwIP fwd decls (keep lwIP headers out of this header)

namespace mc {
class EditorProtocol;
}

namespace mc::mcu {

class WifiManager : public IWifi {
public:
    explicit WifiManager(IConfigStore& store, uint16_t tcpPort = 8080);

    void setProtocol(EditorProtocol* proto) { proto_ = proto; }
    void setStatusSink(std::function<void(const std::string&)> sink) { statusSink_ = std::move(sink); }

    void begin();  // cyw43_arch_init + load config + connect if enabled
    void poll();   // service cyw43/lwIP (call every main-loop iteration)

    // IWifi
    void setCredentials(const std::string& ssid, const std::string& password) override;
    void setEnabled(bool on) override;
    WifiStatus status() const override;

    // TCP raw-API callbacks (public so the C trampolines can reach them).
    void onAccept(tcp_pcb* newpcb);
    void onRecv(tcp_pcb* pcb, const void* data, uint16_t len, bool closed);
    void onSent();
    void onClientErr();

private:
    void loadConfig();
    void saveConfig();
    void connectNow();
    void disconnectNow();
    void startServer();
    void startMdns();
    void emitStatus(const std::string& msg);
    void trySend();

    IConfigStore& store_;
    EditorProtocol* proto_ = nullptr;
    std::function<void(const std::string&)> statusSink_;
    uint16_t port_;

    bool enabled_ = false;
    std::string ssid_;
    std::string password_;
    bool connected_ = false;
    std::string ip_;
    bool inited_ = false;
    bool mdnsUp_ = false;

    tcp_pcb* listen_ = nullptr;
    tcp_pcb* client_ = nullptr;
    std::string rxLine_;
    std::string outbox_;
    size_t outSent_ = 0;
};

}  // namespace mc::mcu
