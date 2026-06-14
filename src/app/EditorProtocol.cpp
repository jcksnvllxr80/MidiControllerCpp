#include "mc/app/EditorProtocol.h"

#include <cstdlib>
#include <vector>

#include <nlohmann/json.hpp>

namespace mc {

using json = nlohmann::ordered_json;

namespace {
json toArray(const std::vector<std::string>& v) {
    json a = json::array();
    for (const auto& s : v) a.push_back(s);
    return a;
}
}  // namespace

EditorProtocol::EditorProtocol(IConfigStore& store, Application& app, std::string deviceName,
                               std::string firmware, int protocolVersion)
    : store_(store),
      app_(app),
      name_(std::move(deviceName)),
      firmware_(std::move(firmware)),
      version_(protocolVersion) {}

std::string EditorProtocol::handleLine(const std::string& line) {
    if (line.find_first_not_of(" \t\r\n") == std::string::npos) return "";  // blank

    json req;
    try {
        req = json::parse(line);
    } catch (...) {
        return "";  // not JSON -> device-log/noise line, nothing to answer
    }
    if (!req.is_object()) return "";

    const long long id = req.value("id", 0LL);
    const std::string op = req.value("op", std::string());

    json resp;
    resp["id"] = id;
    try {
        auto name = [&] { return req.at("name").get<std::string>(); };
        if (op == "identify") {
            resp["ok"] = true;
            resp["data"] = {{"name", name_}, {"firmware", firmware_}, {"protocol_version", version_}};
        } else if (op == "ping") {
            resp["ok"] = true;
        } else if (op == "list_sets") {
            resp["ok"] = true;
            resp["data"] = toArray(store_.list("sets"));
        } else if (op == "list_songs") {
            resp["ok"] = true;
            resp["data"] = toArray(store_.list("songs"));
        } else if (op == "list_pedals") {
            resp["ok"] = true;
            resp["data"] = toArray(store_.list("pedals"));
        } else if (op == "get_set") {
            resp["ok"] = true;
            resp["data"] = json::parse(store_.read("sets/" + name() + ".json"));
        } else if (op == "get_song") {
            resp["ok"] = true;
            resp["data"] = json::parse(store_.read("songs/" + name() + ".json"));
        } else if (op == "get_pedal") {
            resp["ok"] = true;
            resp["data"] = json::parse(store_.read("pedals/" + name() + ".json"));
        } else if (op == "write_set") {
            store_.write("sets/" + name() + ".json", req.at("data").dump(2));
            resp["ok"] = true;
        } else if (op == "write_song") {
            store_.write("songs/" + name() + ".json", req.at("data").dump(2));
            resp["ok"] = true;
        } else if (op == "write_pedal") {
            store_.write("pedals/" + name() + ".json", req.at("data").dump(2));
            resp["ok"] = true;
        } else if (op == "dpad") {
            const std::string dir = req.at("direction").get<std::string>();
            if (dir == "CW") app_.handleEvent({InputEvent::Type::EncoderCW, 0, 0.0});
            else if (dir == "CCW") app_.handleEvent({InputEvent::Type::EncoderCCW, 0, 0.0});
            else if (dir == "up") app_.handleEvent({InputEvent::Type::RotaryPress, 0, 1.0});    // long
            else if (dir == "down") app_.handleEvent({InputEvent::Type::RotaryPress, 0, 0.2});  // short
            resp["ok"] = true;
            resp["data"] = {{"display_message", app_.displayedMessage()}};
        } else if (op == "short") {
            int button = std::atoi(req.at("button").get<std::string>().c_str());
            app_.handleEvent({InputEvent::Type::FootswitchShort, button, 0.0});
            resp["ok"] = true;
            resp["data"] = {{"display_message", app_.displayedMessage()}};
        } else {
            resp["ok"] = false;
            resp["error"] = "unsupported op: " + op;
        }
    } catch (const std::exception& e) {
        resp = json{{"id", id}, {"ok", false}, {"error", std::string(e.what())}};
    }
    return resp.dump() + "\n";
}

}  // namespace mc
