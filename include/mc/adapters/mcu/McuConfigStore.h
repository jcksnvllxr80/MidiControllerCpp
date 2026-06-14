#pragma once
//
// McuConfigStore — IConfigStore backed by config baked into flash at build time
// (data/*.json embedded as blobs by tools/embed_data.py). Reads come from the
// embedded table; writes go to a RAM overlay that shadows the embedded blobs.
//
// Pass an IKeyValueStore (e.g. FlashKv) to make writes survive reboot: the
// overlay is loaded from it on construction and re-saved on every write().
//
#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "mc/adapters/mcu/IKeyValueStore.h"
#include "mc/ports/IConfigStore.h"

namespace mc::mcu {

struct EmbeddedBlob {
    const char* key;   // e.g. "pedals/TimeLine.json"
    const char* data;  // JSON text
    size_t len;
};

class McuConfigStore : public IConfigStore {
public:
    McuConfigStore(const EmbeddedBlob* table, size_t count, IKeyValueStore* persist = nullptr);

    bool exists(const std::string& key) const override;
    std::string read(const std::string& key) const override;
    void write(const std::string& key, const std::string& data) override;  // overlay (+ persist)
    void remove(const std::string& key) override;                          // tombstone (+ persist)
    std::vector<std::string> list(const std::string& subdir) const override;

private:
    const EmbeddedBlob* table_;
    size_t count_;
    IKeyValueStore* persist_;
    std::map<std::string, std::string> overlay_;
};

}  // namespace mc::mcu
