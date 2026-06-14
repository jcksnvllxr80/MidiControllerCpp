#include "mc/adapters/mcu/WifiManager.h"

#include <algorithm>

#include "pico/cyw43_arch.h"

#include "lwip/apps/mdns.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#include <nlohmann/json.hpp>

#include "mc/app/EditorProtocol.h"

namespace mc::mcu {

namespace {
using json = nlohmann::json;

err_t tcpRecvTramp(void* arg, tcp_pcb* pcb, pbuf* p, err_t err) {
    auto* self = static_cast<WifiManager*>(arg);
    if (p == nullptr) {
        self->onRecv(pcb, nullptr, 0, true);
        return ERR_OK;
    }
    if (err == ERR_OK) {
        for (pbuf* q = p; q; q = q->next) self->onRecv(pcb, q->payload, q->len, false);
        tcp_recved(pcb, p->tot_len);
    }
    pbuf_free(p);
    return ERR_OK;
}
err_t tcpSentTramp(void* arg, tcp_pcb*, u16_t) {
    static_cast<WifiManager*>(arg)->onSent();
    return ERR_OK;
}
void tcpErrTramp(void* arg, err_t) { static_cast<WifiManager*>(arg)->onClientErr(); }
err_t tcpAcceptTramp(void* arg, tcp_pcb* newpcb, err_t err) {
    if (err != ERR_OK || newpcb == nullptr) return ERR_VAL;
    static_cast<WifiManager*>(arg)->onAccept(newpcb);
    return ERR_OK;
}
void mdnsSrvTxt(struct mdns_service* service, void* /*txt_userdata*/) {
    mdns_resp_add_service_txtitem(service, "version=1", 9);
}
}  // namespace

WifiManager::WifiManager(IConfigStore& store, uint16_t tcpPort) : store_(store), port_(tcpPort) {}

void WifiManager::emitStatus(const std::string& msg) {
    if (statusSink_) statusSink_(msg);
}

void WifiManager::loadConfig() {
    if (!store_.exists("wifi.json")) return;
    try {
        auto j = json::parse(store_.read("wifi.json"));
        ssid_ = j.value("ssid", std::string());
        password_ = j.value("password", std::string());
        enabled_ = j.value("enabled", false);
    } catch (...) {
        // corrupt config -> ignore, stay off
    }
}

void WifiManager::saveConfig() {
    json j;
    j["ssid"] = ssid_;
    j["password"] = password_;
    j["enabled"] = enabled_;
    store_.write("wifi.json", j.dump());
}

void WifiManager::begin() {
    loadConfig();
    if (cyw43_arch_init()) {
        emitStatus("WiFi: init failed");
        return;
    }
    inited_ = true;
    if (enabled_ && !ssid_.empty()) connectNow();
}

void WifiManager::poll() {
    if (inited_) cyw43_arch_poll();
}

void WifiManager::connectNow() {
    if (!inited_ || !enabled_ || ssid_.empty()) return;
    cyw43_arch_enable_sta_mode();
    emitStatus("WiFi: connecting");
    uint32_t auth = password_.empty() ? CYW43_AUTH_OPEN : CYW43_AUTH_WPA2_AES_PSK;
    if (cyw43_arch_wifi_connect_timeout_ms(ssid_.c_str(), password_.c_str(), auth, 20000)) {
        connected_ = false;
        ip_.clear();
        emitStatus("WiFi: connect failed");
        return;
    }
    connected_ = true;
    ip_ = ip4addr_ntoa(netif_ip4_addr(netif_default));
    startServer();
    startMdns();
    emitStatus("WiFi " + ip_);
}

void WifiManager::disconnectNow() {
    if (client_) {
        tcp_arg(client_, nullptr);
        tcp_close(client_);
        client_ = nullptr;
    }
    if (listen_) {
        tcp_close(listen_);
        listen_ = nullptr;
    }
    rxLine_.clear();
    outbox_.clear();
    outSent_ = 0;
    if (inited_) cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
    connected_ = false;
    ip_.clear();
    emitStatus("WiFi: off");
}

void WifiManager::startServer() {
    if (listen_) return;
    tcp_pcb* p = tcp_new_ip_type(IPADDR_TYPE_ANY);
    if (!p) return;
    if (tcp_bind(p, IP_ANY_TYPE, port_) != ERR_OK) {
        tcp_close(p);
        return;
    }
    listen_ = tcp_listen_with_backlog(p, 1);
    if (!listen_) {
        tcp_close(p);
        return;
    }
    tcp_arg(listen_, this);
    tcp_accept(listen_, tcpAcceptTramp);
}

void WifiManager::startMdns() {
    if (mdnsUp_) return;
    mdns_resp_init();
    mdns_resp_add_netif(netif_default, "midicontroller");
    mdns_resp_add_service(netif_default, "MidiController", "_midicontroller", DNSSD_PROTO_TCP, port_, mdnsSrvTxt,
                          nullptr);
    mdnsUp_ = true;
}

void WifiManager::onAccept(tcp_pcb* newpcb) {
    if (client_) {  // single client: replace the old one
        tcp_arg(client_, nullptr);
        tcp_close(client_);
    }
    client_ = newpcb;
    rxLine_.clear();
    outbox_.clear();
    outSent_ = 0;
    tcp_arg(client_, this);
    tcp_recv(client_, tcpRecvTramp);
    tcp_sent(client_, tcpSentTramp);
    tcp_err(client_, tcpErrTramp);
}

void WifiManager::onRecv(tcp_pcb* pcb, const void* data, uint16_t len, bool closed) {
    if (closed) {
        if (client_ == pcb) {
            tcp_arg(pcb, nullptr);
            tcp_close(pcb);
            client_ = nullptr;
            rxLine_.clear();
            outbox_.clear();
            outSent_ = 0;
        }
        return;
    }
    const char* p = static_cast<const char*>(data);
    for (uint16_t i = 0; i < len; ++i) {
        char c = p[i];
        if (c == '\n') {
            if (proto_) outbox_ += proto_->handleLine(rxLine_);
            rxLine_.clear();
        } else if (c != '\r' && rxLine_.size() < 128 * 1024) {
            rxLine_.push_back(c);
        }
    }
    trySend();
}

void WifiManager::trySend() {
    if (!client_) return;
    while (outSent_ < outbox_.size()) {
        u16_t avail = tcp_sndbuf(client_);
        if (avail == 0) break;
        u16_t n = static_cast<u16_t>(std::min<size_t>(avail, outbox_.size() - outSent_));
        if (tcp_write(client_, outbox_.data() + outSent_, n, TCP_WRITE_FLAG_COPY) != ERR_OK) break;
        outSent_ += n;
    }
    tcp_output(client_);
    if (outSent_ >= outbox_.size()) {
        outbox_.clear();
        outSent_ = 0;
    }
}

void WifiManager::onSent() { trySend(); }

void WifiManager::onClientErr() {
    client_ = nullptr;  // lwIP already freed the pcb
    rxLine_.clear();
    outbox_.clear();
    outSent_ = 0;
}

void WifiManager::setCredentials(const std::string& ssid, const std::string& password) {
    ssid_ = ssid;
    password_ = password;
    saveConfig();
    if (inited_ && enabled_) {
        disconnectNow();  // drop old link state, keep enabled_
        enabled_ = true;
        connectNow();
    }
}

void WifiManager::setEnabled(bool on) {
    enabled_ = on;
    saveConfig();
    if (!inited_) return;
    if (on) connectNow();
    else disconnectNow();
}

WifiStatus WifiManager::status() const {
    WifiStatus s;
    s.enabled = enabled_;
    s.connected = connected_;
    s.ssid = ssid_;
    s.ip = ip_;
    return s;
}

}  // namespace mc::mcu
