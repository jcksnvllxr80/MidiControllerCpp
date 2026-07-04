#include "mc/adapters/mcu/FlashKv.h"

#include <cstring>
#include <vector>

#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/flash.h"   // flash_safe_execute
#include "pico/stdlib.h"  // XIP_BASE, PICO_OK

#include "mc/adapters/mcu/KvCodec.h"
#include "mc/adapters/mcu/Log.h"

namespace mc::mcu {

namespace {
struct FlashOp {
    uint32_t offset;
    uint32_t size;
    const uint8_t* data;
    size_t len;
};
// Runs with IRQs disabled (and, on a multicore build, the other core parked).
// Must touch nothing but flash and its own argument — no heap, no logging.
void doFlashOp(void* arg) {
    auto* op = static_cast<FlashOp*>(arg);
    flash_range_erase(op->offset, op->size);
    flash_range_program(op->offset, op->data, op->len);
}
}  // namespace

FlashKv::FlashKv(uint32_t flashOffset, uint32_t sizeBytes) : offset_(flashOffset), size_(sizeBytes) {}

std::map<std::string, std::string> FlashKv::load() {
    const uint8_t* mapped = reinterpret_cast<const uint8_t*>(XIP_BASE + offset_);
    return kv::decode(mapped, size_);  // empty if erased (0xFF) or corrupt
}

void FlashKv::save(const std::map<std::string, std::string>& kv) {
    std::string blob = kv::encode(kv);
    if (blob.size() > size_) {
        LOG_E("flash", "save: blob %zu bytes exceeds sector %lu — skipped",
              blob.size(), static_cast<unsigned long>(size_));
        return;
    }

    // Pad up to a whole number of flash pages (program granularity).
    size_t padded = ((blob.size() + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE) * FLASH_PAGE_SIZE;
    std::vector<uint8_t> buf(padded, 0xFF);
    std::memcpy(buf.data(), blob.data(), blob.size());

    LOG_D("flash", "save %zu bytes -> %zu padded (offset=0x%08lX)",
          blob.size(), padded, static_cast<unsigned long>(offset_));

    FlashOp op{offset_, size_, buf.data(), buf.size()};
    // flash_safe_execute is the SDK-blessed path: it parks the other core and
    // disables IRQs so nothing executes from flash (XIP) during erase/program.
    // This build never launches core1, so PICO_FLASH_ASSUME_CORE1_SAFE=1 (set in
    // CMakeLists) lets it proceed without a registered lockout victim. If you ever
    // start core1, drop that define and call multicore_lockout_victim_init() there.
    if (flash_safe_execute(doFlashOp, &op, 500) != PICO_OK) {
        // Fallback if the safe path is unavailable: mask IRQs and do it directly.
        // Correct as long as nothing else executes from flash meanwhile (single core).
        LOG_W("flash", "flash_safe_execute unavailable — falling back to direct IRQ-masked write");
        uint32_t ints = save_and_disable_interrupts();
        doFlashOp(&op);
        restore_interrupts(ints);
    }
    LOG_I("flash", "save done");
}

}  // namespace mc::mcu
