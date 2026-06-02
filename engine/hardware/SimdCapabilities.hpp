#pragma once

#include <cstddef>
#include <cstdint>

namespace trading {

// Runtime SIMD feature detection.
// Probed once at startup via CPUID; results are cached.
class SimdCapabilities {
public:
    static bool has_sse42()  noexcept;
    static bool has_avx2()   noexcept;
    static bool has_avx512() noexcept;

    // Human-readable name of the best available level.
    static const char* best() noexcept;
};

// ── Vectorized price-level scan ───────────────────────────────────────────────
//
// Finds the index of the first non-zero value in a uint64_t array.
// Used to locate best_bid / best_ask without a branch-heavy loop.
//
// AVX2 path:  processes 4 × uint64_t per iteration (256-bit vectors).
// Scalar path: simple linear scan — identical results, used when AVX2
//              is unavailable or the array is short.
//
// Returns the index of the first non-zero element, or `count` if all zero.
std::size_t find_first_nonzero(const uint64_t* data,
                                std::size_t    count) noexcept;

// Finds the index of the last non-zero value (for best_bid scan, descending).
// Returns count if all zero.
std::size_t find_last_nonzero(const uint64_t* data,
                               std::size_t    count) noexcept;

} // namespace trading
