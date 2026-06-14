#pragma once
//
// IKeyValueStore — tiny persistence backend for McuConfigStore's write overlay.
// Swappable so the flash codec is host-testable with a fake.
//
#include <map>
#include <string>

namespace mc::mcu {

class IKeyValueStore {
public:
    virtual ~IKeyValueStore() = default;
    virtual std::map<std::string, std::string> load() = 0;
    virtual void save(const std::map<std::string, std::string>& kv) = 0;
};

}  // namespace mc::mcu
