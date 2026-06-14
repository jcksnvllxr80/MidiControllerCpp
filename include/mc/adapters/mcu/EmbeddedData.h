#pragma once
//
// Declares the config blobs baked into flash. The definition (EmbeddedData.cpp)
// is GENERATED from data/*.json by tools/embed_data.py (the CMake build runs it).
//
#include <cstddef>

#include "mc/adapters/mcu/McuConfigStore.h"

namespace mc::mcu {

extern const EmbeddedBlob kEmbeddedData[];
extern const size_t kEmbeddedDataCount;

}  // namespace mc::mcu
