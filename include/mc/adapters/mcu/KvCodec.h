#pragma once
//
// KvCodec — serialize/parse a string->string map to a flat byte blob:
//   [u32 magic][u32 count]( [u32 keyLen][key][u32 valLen][val] )*
// Little-endian. decode() returns empty on bad magic (erased flash reads 0xFF)
// or truncation. Pure (no SDK) so it's unit-tested on the host.
//
#include <cstdint>
#include <map>
#include <string>

namespace mc::mcu::kv {

constexpr uint32_t kMagic = 0x4D434B56;  // 'MCKV'

inline void putU32(std::string& s, uint32_t v) {
    s.push_back(static_cast<char>(v & 0xFF));
    s.push_back(static_cast<char>((v >> 8) & 0xFF));
    s.push_back(static_cast<char>((v >> 16) & 0xFF));
    s.push_back(static_cast<char>((v >> 24) & 0xFF));
}

inline bool getU32(const uint8_t* d, size_t len, size_t& i, uint32_t& out) {
    if (i + 4 > len) return false;
    out = static_cast<uint32_t>(d[i]) | (static_cast<uint32_t>(d[i + 1]) << 8) |
          (static_cast<uint32_t>(d[i + 2]) << 16) | (static_cast<uint32_t>(d[i + 3]) << 24);
    i += 4;
    return true;
}

inline std::string encode(const std::map<std::string, std::string>& m) {
    std::string s;
    putU32(s, kMagic);
    putU32(s, static_cast<uint32_t>(m.size()));
    for (const auto& kv : m) {
        putU32(s, static_cast<uint32_t>(kv.first.size()));
        s += kv.first;
        putU32(s, static_cast<uint32_t>(kv.second.size()));
        s += kv.second;
    }
    return s;
}

inline std::map<std::string, std::string> decode(const uint8_t* d, size_t len) {
    std::map<std::string, std::string> m;
    size_t i = 0;
    uint32_t magic = 0, count = 0;
    if (!getU32(d, len, i, magic) || magic != kMagic) return m;
    if (!getU32(d, len, i, count)) return m;
    for (uint32_t e = 0; e < count; ++e) {
        uint32_t kl = 0, vl = 0;
        if (!getU32(d, len, i, kl) || i + kl > len) return {};
        std::string key(reinterpret_cast<const char*>(d) + i, kl);
        i += kl;
        if (!getU32(d, len, i, vl) || i + vl > len) return {};
        std::string val(reinterpret_cast<const char*>(d) + i, vl);
        i += vl;
        m[key] = val;
    }
    return m;
}

}  // namespace mc::mcu::kv
