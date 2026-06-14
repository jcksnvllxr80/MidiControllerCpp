#pragma once
//
// IConfigStore — persistence for config blobs (the controller config, pedal
// command maps, songs, sets). Keys are relative paths like "pedals/TimeLine.json"
// or "songs/Victory - Rhythm.json". The sim backs this with the filesystem;
// Phase 3 backs it with flash/SD. Content is opaque JSON text — parsing is the
// config layer's job.
//
#include <string>
#include <vector>

namespace mc {

class IConfigStore {
public:
    virtual ~IConfigStore() = default;

    virtual bool exists(const std::string& key) const = 0;
    virtual std::string read(const std::string& key) const = 0;
    virtual void write(const std::string& key, const std::string& data) = 0;
    // Delete an entry. Idempotent: removing a missing key is a no-op. After this,
    // exists(key) is false and list() no longer reports it (the flash-backed store
    // tombstones build-time-embedded entries, since those can't be physically erased).
    virtual void remove(const std::string& key) = 0;
    // List the stems (no extension) of entries under a logical subdir, e.g.
    // list("sets") -> {"2016-09-16 Set", "2017-05-22 Set", ...}.
    virtual std::vector<std::string> list(const std::string& subdir) const = 0;
};

}  // namespace mc
