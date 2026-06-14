#include "mc/adapters/mcu/FlashKv.h"

#include <cstring>
#include <vector>

#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"  // XIP_BASE

#include "mc/adapters/mcu/KvCodec.h"

namespace mc::mcu {

FlashKv::FlashKv(uint32_t flashOffset, uint32_t sizeBytes) : offset_(flashOffset), size_(sizeBytes) {}

std::map<std::string, std::string> FlashKv::load() {
    const uint8_t* mapped = reinterpret_cast<const uint8_t*>(XIP_BASE + offset_);
    return kv::decode(mapped, size_);  // empty if erased (0xFF) or corrupt
}

void FlashKv::save(const std::map<std::string, std::string>& kv) {
    std::string blob = kv::encode(kv);
    if (blob.size() > size_) return;  // too big for the reserved sector (TODO: multi-sector)

    // Pad up to a whole number of flash pages (program granularity).
    size_t padded = ((blob.size() + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE) * FLASH_PAGE_SIZE;
    std::vector<uint8_t> buf(padded, 0xFF);
    std::memcpy(buf.data(), blob.data(), blob.size());

    // NOTE: on dual-core, the other core must not execute from flash during this.
    // For more safety use flash_safe_execute(); a single-core save path just masks IRQs.
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(offset_, size_);
    flash_range_program(offset_, buf.data(), buf.size());
    restore_interrupts(ints);
}

}  // namespace mc::mcu
