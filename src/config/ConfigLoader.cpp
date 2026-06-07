#include "mc/config/ConfigLoader.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace mc::config {

using json = nlohmann::ordered_json;

namespace {

long asLong(const json& v) { return static_cast<long>(v.get<long long>()); }

// Map a JSON scalar/object to a domain Value, mirroring Python duck typing.
Value jsonToValue(const json& j) {
    if (j.is_null()) return std::monostate{};
    if (j.is_boolean()) return j.get<bool>();
    if (j.is_number_integer() || j.is_number_unsigned()) return asLong(j);
    if (j.is_number_float()) return static_cast<long>(j.get<double>());
    if (j.is_string()) return j.get<std::string>();
    if (j.is_object()) {  // {name, engaged} param values (SuperSwitcher loops)
        EngagedRef e;
        if (j.contains("name") && j["name"].is_string()) e.name = j["name"].get<std::string>();
        if (j.contains("engaged")) e.engaged = j["engaged"].get<bool>();
        return e;
    }
    return std::monostate{};
}

SubAction parseSubAction(const json& j) {
    SubAction s;
    if (j.is_object()) {
        if (j.contains("func") && j["func"].is_string()) s.transform = Transform::parse(j["func"].get<std::string>());
        if (j.contains("min")) s.min = asLong(j["min"]);
        if (j.contains("max")) s.max = asLong(j["max"]);
        if (j.contains("options") && j["options"].is_array())
            for (const auto& o : j["options"]) s.options.push_back(o.get<std::string>());
    }
    return s;
}

std::vector<std::string> parseMulti(const json& j) {
    std::vector<std::pair<int, std::string>> tmp;
    if (j.is_object()) {
        for (auto it = j.begin(); it != j.end(); ++it) tmp.push_back({std::stoi(it.key()), it.value().get<std::string>()});
        std::sort(tmp.begin(), tmp.end(), [](auto& a, auto& b) { return a.first < b.first; });
    } else if (j.is_array()) {
        int i = 0;
        for (const auto& e : j) tmp.push_back({i++, e.get<std::string>()});
    }
    std::vector<std::string> out;
    for (auto& kv : tmp) out.push_back(kv.second);
    return out;
}

Action parseAction(const json& j) {
    Action a;
    if (!j.is_object()) return a;
    for (auto it = j.begin(); it != j.end(); ++it) {
        const std::string& key = it.key();
        const json& val = it.value();
        if (key == "cc") a.cc = asLong(val);
        else if (key == "pc") a.pc = asLong(val);
        else if (key == "value") { if (val.is_number_integer() || val.is_number_unsigned()) a.value = asLong(val); }
        else if (key == "on") a.on = asLong(val);
        else if (key == "off") a.off = asLong(val);
        else if (key == "press") a.press = asLong(val);
        else if (key == "release") a.release = asLong(val);
        else if (key == "min") a.min = asLong(val);
        else if (key == "max") a.max = asLong(val);
        else if (key == "func") a.func = Transform::parse(val.get<std::string>());
        else if (key == "dict") {
            for (auto d = val.begin(); d != val.end(); ++d) a.dict.push_back({d.key(), asLong(d.value())});
        } else if (key == "program change") a.programChange = parseSubAction(val);
        else if (key == "control change") a.controlChange = parseSubAction(val);
        else if (key == "multi") a.multi = parseMulti(val);
        else if (key == "notes" || key == "display" || key == "options") { /* metadata, ignore */ }
        else a.children.push_back({key, parseAction(val)});  // named sub-action
    }
    return a;
}

}  // namespace

std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("ConfigLoader: cannot open '" + path + "'");
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// --- pedal config ------------------------------------------------------------

PedalConfig loadPedalConfigFromString(const std::string& jsonText) {
    PedalConfig cfg;
    cfg.root = parseAction(json::parse(jsonText));
    return cfg;
}

PedalConfig loadPedalConfigFromFile(const std::string& path) {
    return loadPedalConfigFromString(readFile(path));
}

// --- songs / sets ------------------------------------------------------------

Song loadSongFromString(const std::string& jsonText, const std::string& fallbackName) {
    json j = json::parse(jsonText);
    Song song;
    song.name = j.value("name", fallbackName);
    if (j.contains("tempo")) {
        const json& t = j["tempo"];
        if (t.is_string()) song.bpm = t.get<std::string>();
        else if (t.is_number_integer() || t.is_number_unsigned()) song.bpm = std::to_string(asLong(t));
        else if (t.is_number_float()) song.bpm = std::to_string(t.get<double>());
    }

    std::vector<std::pair<int, Part>> parts;
    if (j.contains("parts") && j["parts"].is_object()) {
        const json& pj = j["parts"];
        int fallbackPos = 1;
        for (auto it = pj.begin(); it != pj.end(); ++it) {
            const json& partj = it.value();
            Part part;
            part.name = it.key();
            int pos = partj.value("position", fallbackPos);
            if (partj.contains("pedals") && partj["pedals"].is_object()) {
                const json& peds = partj["pedals"];
                for (auto pit = peds.begin(); pit != peds.end(); ++pit) {
                    const json& ped = pit.value();
                    PedalState st;
                    st.engaged = ped.value("engaged", false);
                    if (ped.contains("preset") && !ped["preset"].is_null()) st.preset = jsonToValue(ped["preset"]);
                    else st.preset = std::string("");  // Python default '' (no-op on load)
                    if (ped.contains("params") && ped["params"].is_object()) {
                        const json& prm = ped["params"];
                        for (auto mit = prm.begin(); mit != prm.end(); ++mit)
                            st.params.push_back({mit.key(), jsonToValue(mit.value())});
                    }
                    if (ped.contains("settings") && !ped["settings"].is_null()) st.settings = jsonToValue(ped["settings"]);
                    else st.settings = std::string("");
                    part.pedals.push_back({pit.key(), st});
                }
            }
            parts.push_back({pos, std::move(part)});
            ++fallbackPos;
        }
        std::sort(parts.begin(), parts.end(), [](auto& a, auto& b) { return a.first < b.first; });
    }
    for (auto& kv : parts) song.parts.push_back(std::move(kv.second));
    return song;
}

Song loadSongFromFile(const std::string& path) { return loadSongFromString(readFile(path)); }

Setlist loadSetlistFromString(const std::string& setJsonText,
                              const std::function<std::string(const std::string&)>& fetchSongJson) {
    json j = json::parse(setJsonText);
    Setlist sl;
    sl.name = j.value("name", "");
    if (j.contains("songs") && j["songs"].is_array()) {
        for (const auto& sn : j["songs"]) {
            std::string songName = sn.get<std::string>();
            Song song = loadSongFromString(fetchSongJson(songName), songName);
            song.name = songName;  // Python uses the set's listed name as the Song name
            sl.songs.push_back(std::move(song));
        }
    }
    return sl;
}

Setlist loadSetlistFromFile(const std::string& setFilePath, const std::string& songsDir) {
    return loadSetlistFromString(readFile(setFilePath), [&](const std::string& songName) {
        return readFile(songsDir + "/" + songName + ".json");
    });
}

// --- controller config -------------------------------------------------------

ControllerState loadControllerStateFromString(const std::string& jsonText) {
    json j = json::parse(jsonText);
    ControllerState s;

    if (j.contains("controller_api")) {
        const json& ca = j["controller_api"];
        s.buttonsLocked = ca.value("buttons_locked", false);
        s.apiPort = ca.value("port", 8090);
    }
    if (j.contains("current_settings")) {
        const json& cs = j["current_settings"];
        s.mode = cs.value("mode", "standard");
        if (cs.contains("tempo")) {
            const json& t = cs["tempo"];
            if (t.is_number_integer() || t.is_number_unsigned()) s.tempo = static_cast<int>(asLong(t));
            else if (t.is_string()) s.tempo = std::stoi(t.get<std::string>());
        }
        if (cs.contains("preset")) {
            const json& pr = cs["preset"];
            s.currentPart = pr.value("part", "");
            s.currentSet = pr.value("setList", "");
            s.currentSong = pr.value("song", "");
        }
    }
    if (j.contains("knob")) {
        const json& k = j["knob"];
        s.knobColor = k.value("color", "Off");
        if (k.contains("brightness")) {
            const json& b = k["brightness"];
            if (b.is_string()) s.knobBrightness = std::stoi(b.get<std::string>());
            else if (b.is_number_integer() || b.is_number_unsigned()) s.knobBrightness = static_cast<int>(asLong(b));
        }
    }
    if (j.contains("midi") && j["midi"].contains("channels")) {
        const json& ch = j["midi"]["channels"];
        for (auto it = ch.begin(); it != ch.end(); ++it) {
            const json& cj = it.value();
            if (cj.is_null()) continue;
            ChannelConfig c;
            c.channel = std::stoi(it.key());
            c.name = cj.value("name", "");
            c.state = cj.value("state", false);
            if (cj.contains("preset")) {
                const json& pp = cj["preset"];
                if (pp.contains("number")) c.preset = jsonToValue(pp["number"]);
                else if (pp.contains("name")) c.preset = jsonToValue(pp["name"]);
            }
            s.channels.push_back(std::move(c));
        }
        std::sort(s.channels.begin(), s.channels.end(), [](auto& a, auto& b) { return a.channel < b.channel; });
    }
    if (j.contains("button_setup")) {
        const json& bs = j["button_setup"];
        for (auto it = bs.begin(); it != bs.end(); ++it) {
            const json& bj = it.value();
            ButtonConfig bc;
            bc.button = std::stoi(it.key());
            bc.function = bj.value("function", "");
            bc.longPressFunc = bj.value("long_press_func", "");
            bc.partnerFunc = bj.value("partner_func", "");
            s.buttons.push_back(std::move(bc));
        }
        std::sort(s.buttons.begin(), s.buttons.end(), [](auto& a, auto& b) { return a.button < b.button; });
    }
    return s;
}

ControllerState loadControllerStateFromFile(const std::string& path) {
    return loadControllerStateFromString(readFile(path));
}

}  // namespace mc::config
