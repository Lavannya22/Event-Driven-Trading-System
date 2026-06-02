#include "engine/hardware/SimdCapabilities.hpp"

#if defined(__x86_64__) || defined(_M_X64)
#  define HAVE_X86
#  include <cpuid.h>
#  ifdef __AVX2__
#    include <immintrin.h>
#    define USE_AVX2
#  endif
#endif

namespace trading {

// ── CPUID probing ─────────────────────────────────────────────────────────────

namespace {
    struct CpuFeatures {
        bool sse42{false};
        bool avx2{false};
        bool avx512f{false};

        CpuFeatures() noexcept {
#ifdef HAVE_X86
            unsigned eax, ebx, ecx, edx;

            // Leaf 1: SSE4.2 (ecx bit 20)
            if (__get_cpuid(1, &eax, &ebx, &ecx, &edx))
                sse42 = (ecx >> 20) & 1u;

            // Leaf 7, subleaf 0: AVX2 (ebx bit 5), AVX-512F (ebx bit 16)
            if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
                avx2   = (ebx >> 5)  & 1u;
                avx512f= (ebx >> 16) & 1u;
            }
#endif
        }
    };

    const CpuFeatures& features() noexcept {
        static const CpuFeatures f;
        return f;
    }
} // namespace

bool SimdCapabilities::has_sse42()  noexcept { return features().sse42;   }
bool SimdCapabilities::has_avx2()   noexcept { return features().avx2;    }
bool SimdCapabilities::has_avx512() noexcept { return features().avx512f; }

const char* SimdCapabilities::best() noexcept {
    if (has_avx512()) return "AVX-512";
    if (has_avx2())   return "AVX2";
    if (has_sse42())  return "SSE4.2";
    return "scalar";
}

// ── find_first_nonzero ────────────────────────────────────────────────────────

std::size_t find_first_nonzero(const uint64_t* data,
                                std::size_t    count) noexcept {
#ifdef USE_AVX2
    if (SimdCapabilities::has_avx2() && count >= 4) {
        const __m256i zero = _mm256_setzero_si256();
        std::size_t i = 0;
        for (; i + 4 <= count; i += 4) {
            __m256i v = _mm256_loadu_si256(
                reinterpret_cast<const __m256i*>(data + i));
            __m256i cmp = _mm256_cmpeq_epi64(v, zero);
            int mask = ~_mm256_movemask_epi8(cmp);  // 1 bit per byte, 4 lanes × 8 bytes
            if (mask) {
                // Find which 8-byte lane is non-zero
                for (std::size_t j = 0; j < 4; ++j)
                    if (data[i + j]) return i + j;
            }
        }
        // Handle tail
        for (; i < count; ++i)
            if (data[i]) return i;
        return count;
    }
#endif
    for (std::size_t i = 0; i < count; ++i)
        if (data[i]) return i;
    return count;
}

// ── find_last_nonzero ─────────────────────────────────────────────────────────

std::size_t find_last_nonzero(const uint64_t* data,
                               std::size_t    count) noexcept {
    if (count == 0) return count;
#ifdef USE_AVX2
    if (SimdCapabilities::has_avx2() && count >= 4) {
        const __m256i zero = _mm256_setzero_si256();
        std::size_t tail = count & 3;
        std::size_t i    = count - tail;
        // Scan tail first (highest indices)
        for (std::size_t j = tail; j-- > 0;)
            if (data[i + j]) return i + j;
        // Scan 4-wide chunks from high to low
        while (i >= 4) {
            i -= 4;
            __m256i v = _mm256_loadu_si256(
                reinterpret_cast<const __m256i*>(data + i));
            __m256i cmp = _mm256_cmpeq_epi64(v, zero);
            int mask = ~_mm256_movemask_epi8(cmp);
            if (mask) {
                for (std::size_t j = 4; j-- > 0;)
                    if (data[i + j]) return i + j;
            }
        }
        return count;
    }
#endif
    for (std::size_t i = count; i-- > 0;)
        if (data[i]) return i;
    return count;
}

} // namespace trading
