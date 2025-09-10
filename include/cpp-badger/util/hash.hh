#pragma once

#include <cstddef>
#include <cstdint>

namespace badger {

// Stable/persistent 32-bit hash. Moderate quality and high speed on
// small inputs.
// TODO: consider rename to Hash32
// KNOWN FLAW: incrementing seed by 1 might not give sufficiently independent
// results from previous seed. Recommend pseudorandom or hashed seeds.
extern uint32_t hash(const char* data, size_t n, uint32_t seed);

}  // namespace badger