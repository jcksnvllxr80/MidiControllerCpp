#include "mc/adapters/mcu/McuConfigStore.h"

#include <algorithm>
#include <set>
#include <stdexcept>

namespace mc::mcu {

McuConfigStore::McuConfigStore(const EmbeddedBlob* table, size_t count, IKeyValueStore* persist)
    : table_(table), count_(count), persist_(persist) {
    if (persist_) overlay_ = persist_->load();  // restore saved writes
}

bool McuConfigStore::exists(const std::string& key) const {
    if (overlay_.count(key)) return true;
    for (size_t i = 0; i < count_; ++i)
        if (key == table_[i].key) return true;
    return false;
}

std::string McuConfigStore::read(const std::string& key) const {
    auto it = overlay_.find(key);
    if (it != overlay_.end()) return it->second;
    for (size_t i = 0; i < count_; ++i)
        if (key == table_[i].key) return std::string(table_[i].data, table_[i].len);
    throw std::runtime_error("McuConfigStore: missing '" + key + "'");
}

void McuConfigStore::write(const std::string& key, const std::string& data) {
    overlay_[key] = data;
    if (persist_) persist_->save(overlay_);
}

std::vector<std::string> McuConfigStore::list(const std::string& subdir) const {
    const std::string prefix = subdir + "/";
    const std::string ext = ".json";
    std::set<std::string> stems;  // sorted + deduped
    auto consider = [&](const std::string& key) {
        if (key.size() <= prefix.size() + ext.size()) return;
        if (key.compare(0, prefix.size(), prefix) != 0) return;
        if (key.compare(key.size() - ext.size(), ext.size(), ext) != 0) return;
        stems.insert(key.substr(prefix.size(), key.size() - prefix.size() - ext.size()));
    };
    for (size_t i = 0; i < count_; ++i) consider(table_[i].key);
    for (const auto& kv : overlay_) consider(kv.first);
    return {stems.begin(), stems.end()};
}

}  // namespace mc::mcu
