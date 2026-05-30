#include "engine/runtime/TimestampProvider.hpp"
#include <chrono>
#include <thread>

#if defined(__x86_64__) || defined(_M_X64)
#  include <cpuid.h>
#  define HAVE_X86
#endif

namespace trading {

// ── Chrono ────────────────────────────────────────────────────────────────────

uint64_t ChronoTimestampProvider::now() noexcept {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count());
}

// ── RDTSCP ────────────────────────────────────────────────────────────────────

bool RdtscpTimestampProvider::detect_rdtscp() noexcept {
#ifdef HAVE_X86
    unsigned int eax{}, ebx{}, ecx{}, edx{};
    // CPUID leaf 0x80000001: EDX bit 27 = RDTSCP support
    __cpuid(0x80000001u, eax, ebx, ecx, edx);
    return (edx >> 27) & 1u;
#else
    return false;
#endif
}

uint64_t RdtscpTimestampProvider::read_tsc() noexcept {
#ifdef HAVE_X86
    uint32_t aux;
    uint64_t tsc;
    __asm__ volatile("rdtscp" : "=A"(tsc), "=c"(aux) :: "memory");
    return tsc;
#else
    return 0;
#endif
}

uint64_t RdtscpTimestampProvider::calibrate_tsc_hz() noexcept {
    using Clock = std::chrono::steady_clock;
    using ns    = std::chrono::nanoseconds;

    const uint64_t tsc0 = read_tsc();
    const auto     t0   = Clock::now();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const uint64_t tsc1 = read_tsc();
    const auto     t1   = Clock::now();

    const double elapsed_s =
        static_cast<double>(std::chrono::duration_cast<ns>(t1 - t0).count()) * 1e-9;

    if (elapsed_s <= 0.0) return 0;
    return static_cast<uint64_t>((tsc1 - tsc0) / elapsed_s);
}

RdtscpTimestampProvider::RdtscpTimestampProvider() {
    rdtscp_available_ = detect_rdtscp();
    if (rdtscp_available_)
        tsc_hz_ = calibrate_tsc_hz();
}

uint64_t RdtscpTimestampProvider::now() noexcept {
    if (!rdtscp_available_ || tsc_hz_ == 0) {
        // Fallback to chrono
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count());
    }
    // Convert TSC ticks to nanoseconds: ns = ticks * 1e9 / hz
    return static_cast<uint64_t>(
        static_cast<double>(read_tsc()) * 1'000'000'000.0 /
        static_cast<double>(tsc_hz_));
}

const char* RdtscpTimestampProvider::name() const noexcept {
    return rdtscp_available_ ? "rdtscp" : "rdtscp(fallback→chrono)";
}

} // namespace trading
