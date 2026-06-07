#pragma once
// Shared helpers for tests that exercise the real converted fixtures.
#include <map>
#include <string>
#include <vector>

#include "mc/config/ConfigLoader.h"
#include "mc/domain/PedalConfig.h"

namespace mc::test {

inline const std::vector<std::string>& allPedals() {
    static const std::vector<std::string> v = {"QuartzV2", "TimeLine",      "Iridium",
                                               "BigSky",   "SuperSwitcher", "ScarlettLove"};
    return v;
}

inline const std::vector<std::string>& allSongs() {
    static const std::vector<std::string> v = {"Victory - Rhythm", "Victory - Lead",
                                               "Here As In Heaven - Lead", "Here As In Heaven - Rhythm"};
    return v;
}

inline const std::vector<std::string>& allSets() {
    static const std::vector<std::string> v = {"2016-09-16 Set", "2017-05-22 Set", "2020-09-16 Set"};
    return v;
}

inline int channelFor(const std::string& pedal) {
    static const std::map<std::string, int> m = {{"QuartzV2", 1}, {"TimeLine", 2},      {"Iridium", 3},
                                                 {"BigSky", 4},   {"SuperSwitcher", 5}, {"ScarlettLove", 16}};
    auto it = m.find(pedal);
    return it == m.end() ? 1 : it->second;
}

inline const PedalConfig& pedalConfig(const std::string& name) {
    static std::map<std::string, PedalConfig> cache;
    auto it = cache.find(name);
    if (it == cache.end())
        it = cache.emplace(name, config::loadPedalConfigFromFile("data/pedals/" + name + ".json")).first;
    return it->second;
}

// gtest test-name fragment: keep [A-Za-z0-9_] only.
inline std::string sanitize(std::string s) {
    for (auto& c : s) {
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) c = '_';
    }
    if (s.empty()) s = "x";
    return s;
}

}  // namespace mc::test
