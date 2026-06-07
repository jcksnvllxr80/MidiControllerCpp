#pragma once
//
// Setlist -> Songs -> Parts. Ports PartSongSet.py, but drops the hand-rolled
// doubly-linked list in favour of std::vector; prev/next is just index +/- 1
// (see MenuTree / Application for navigation). A Part stores, per pedal, the
// (engaged, preset, params, settings) tuple it had in the song YAML.
//
#include <string>
#include <utility>
#include <vector>

#include "mc/domain/Value.h"

namespace mc {

struct PedalState {
    bool engaged = false;
    Value preset;                                        // long, string, or empty ("")/None
    std::vector<std::pair<std::string, Value>> params;   // ordered (MIDI order depends on it)
    Value settings;                                      // usually None/empty
};

struct Part {
    std::string name;
    std::vector<std::pair<std::string, PedalState>> pedals;  // song-file order

    const PedalState* pedal(const std::string& pedalName) const {
        for (const auto& kv : pedals)
            if (kv.first == pedalName) return &kv.second;
        return nullptr;
    }
};

struct Song {
    std::string name;
    std::string bpm;            // kept as string like Python (str(tempo))
    std::vector<Part> parts;    // ordered by 'position'

    double bpmValue() const { return bpm.empty() ? 0.0 : std::stod(bpm); }

    const Part* part(const std::string& partName) const {
        for (const auto& p : parts)
            if (p.name == partName) return &p;
        return nullptr;
    }
    int indexOfPart(const std::string& partName) const {
        for (size_t i = 0; i < parts.size(); ++i)
            if (parts[i].name == partName) return static_cast<int>(i);
        return -1;
    }
};

struct Setlist {
    std::string name;
    std::vector<Song> songs;

    const Song* song(const std::string& songName) const {
        for (const auto& s : songs)
            if (s.name == songName) return &s;
        return nullptr;
    }
    int indexOfSong(const std::string& songName) const {
        for (size_t i = 0; i < songs.size(); ++i)
            if (songs[i].name == songName) return static_cast<int>(i);
        return -1;
    }
};

}  // namespace mc
