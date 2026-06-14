#pragma once
//
// FlashKv — IKeyValueStore in one reserved flash sector on the RP2350. Reads via
// the memory-mapped XIP view; writes erase+program the sector. The encode/decode
// is KvCodec (host-tested); only the erase/program here is hardware.
//
// `flashOffset` is relative to flash start (added to XIP_BASE for reads) and must
// be sector-aligned; `sizeBytes` a multiple of FLASH_SECTOR_SIZE.
//
#include <cstdint>

#include "mc/adapters/mcu/IKeyValueStore.h"

namespace mc::mcu {

class FlashKv : public IKeyValueStore {
public:
    FlashKv(uint32_t flashOffset, uint32_t sizeBytes);
    std::map<std::string, std::string> load() override;
    void save(const std::map<std::string, std::string>& kv) override;

private:
    uint32_t offset_;
    uint32_t size_;
};

}  // namespace mc::mcu
