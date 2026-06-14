#include "mc/adapters/mcu/McuConfigStore.h"

#include <algorithm>
#include <set>
#include <stdexcept>

namespace mc::mcu {

namespace {
// Overlay value marking a build-time-embedded key as deleted (the flash blob
// itself can't be erased). Leads with a 0x01 control byte, which dump()ed JSON
// config text never starts with, so it can't collide with real written data.
const std::string kTombstone = std::string(1, '\x01') + "mc:deleted";
inline bool isTombstone(const std::string& v) { return v == kTombstone; }
}  // namespace

McuConfigStore::McuConfigStore(const EmbeddedBlob* table, size_t count, IKeyValueStore* persist)
    : table_(table), count_(count), persist_(persist) {
    if (persist_) overlay_ = persist_->load();  // restore saved writes
}

bool McuConfigStore::exists(const std::string& key) const {
    auto it = overlay_.find(key);
    if (it != overlay_.end()) return !isTombstone(it->second);  // tombstone -> gone
    for (size_t i = 0; i < count_; ++i)
        if (key == table_[i].key) return true;
    return false;
}

std::string McuConfigStore::read(const std::string& key) const {
    auto it = overlay_.find(key);
    if (it != overlay_.end()) {
        if (isTombstone(it->second)) throw std::runtime_error("McuConfigStore: deleted '" + key + "'");
        return it->second;
    }
    for (size_t i = 0; i < count_; ++i)
        if (key == table_[i].key) return std::string(table_[i].data, table_[i].len);
    throw std::runtime_error("McuConfigStore: missing '" + key + "'");
}

void McuConfigStore::write(const std::string& key, const std::string& data) {
    overlay_[key] = data;  // also un-deletes a previously tombstoned key
    if (persist_) persist_->save(overlay_);
}

void McuConfigStore::remove(const std::string& key) {
    bool embedded = false;
    for (size_t i = 0; i < count_; ++i)
        if (key == table_[i].key) { embedded = true; break; }
    if (embedded) overlay_[key] = kTombstone;  // mask the un-erasable embedded blob
    else overlay_.erase(key);                  // pure overlay entry -> just drop it
    if (persist_) persist_->save(overlay_);
}

std::vector<std::string> McuConfigStore::list(const std::string& subdir) const {
    const std::string prefix = subdir + "/";
    const std::string ext = ".json";
    std::set<std::string> stems;  // sorted + deduped
    auto deleted = [&](const std::string& key) {
        auto it = overlay_.find(key);
        return it != overlay_.end() && isTombstone(it->second);
    };
    auto consider = [&](const std::string& key) {
        if (deleted(key)) return;  // tombstoned embedded blob -> don't list it
        if (key.size() <= prefix.size() + ext.size()) return;
        if (key.compare(0, prefix.size(), prefix) != 0) return;
        if (key.compare(key.size() - ext.size(), ext.size(), ext) != 0) return;
        stems.insert(key.substr(prefix.size(), key.size() - prefix.size() - ext.size()));
    };
    for (size_t i = 0; i < count_; ++i) consider(table_[i].key);
    for (const auto& kv : overlay_)
        if (!isTombstone(kv.second)) consider(kv.first);
    return {stems.begin(), stems.end()};
}

}  // namespace mc::mcu
