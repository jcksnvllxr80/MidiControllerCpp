#pragma once
//
// FsConfigStore — filesystem-backed IConfigStore for the sim. Keys are relative
// paths under a base data directory, e.g. "pedals/TimeLine.json".
//
#include <string>
#include <vector>

#include "mc/ports/IConfigStore.h"

namespace mc::sim {

class FsConfigStore : public IConfigStore {
public:
    explicit FsConfigStore(std::string baseDir);

    bool exists(const std::string& key) const override;
    std::string read(const std::string& key) const override;
    void write(const std::string& key, const std::string& data) override;
    void remove(const std::string& key) override;
    std::vector<std::string> list(const std::string& subdir) const override;

private:
    std::string path(const std::string& key) const;
    std::string base_;
};

}  // namespace mc::sim
