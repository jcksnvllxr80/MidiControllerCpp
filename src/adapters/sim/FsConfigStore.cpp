#include "mc/adapters/sim/FsConfigStore.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace mc::sim {

FsConfigStore::FsConfigStore(std::string baseDir) : base_(std::move(baseDir)) {}

std::string FsConfigStore::path(const std::string& key) const { return base_ + "/" + key; }

bool FsConfigStore::exists(const std::string& key) const { return fs::exists(path(key)); }

std::string FsConfigStore::read(const std::string& key) const {
    std::ifstream in(path(key), std::ios::binary);
    if (!in) throw std::runtime_error("FsConfigStore: cannot read '" + key + "'");
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void FsConfigStore::write(const std::string& key, const std::string& data) {
    fs::path p = path(key);
    if (p.has_parent_path()) fs::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("FsConfigStore: cannot write '" + key + "'");
    out << data;
}

void FsConfigStore::remove(const std::string& key) {
    std::error_code ec;
    fs::remove(path(key), ec);  // idempotent: missing file -> false, no throw
}

std::vector<std::string> FsConfigStore::list(const std::string& subdir) const {
    std::vector<std::string> out;
    fs::path dir = path(subdir);
    if (!fs::is_directory(dir)) return out;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json")
            out.push_back(entry.path().stem().string());
    }
    std::sort(out.begin(), out.end());
    return out;
}

}  // namespace mc::sim
