#pragma once

#include <cstdint>

namespace trading {

// Abstract nanosecond timestamp source.
// Phase 2 provides two implementations:
//   ChronoTimestampProvider  — portable, uses std::chrono::steady_clock
//   RdtscpTimestampProvider  — RDTSCP instruction with TSC frequency calibration;
//                              falls back to chrono if the CPU doesn't support RDTSCP.
class TimestampProvider {
public:
    virtual ~TimestampProvider() = default;
    virtual uint64_t now() noexcept = 0;
    virtual const char* name() const noexcept = 0;
};

// ── Chrono implementation ─────────────────────────────────────────────────────

class ChronoTimestampProvider final : public TimestampProvider {
public:
    uint64_t now() noexcept override;
    const char* name() const noexcept override { return "chrono"; }
};

// ── RDTSCP implementation ─────────────────────────────────────────────────────
// Uses the RDTSCP instruction for sub-nanosecond overhead timestamps.
// Automatically falls back to chrono on CPUs that don't support RDTSCP,
// or on non-x86 platforms.

class RdtscpTimestampProvider final : public TimestampProvider {
public:
    // Calibrates the TSC frequency on construction.
    // If RDTSCP is unavailable the provider silently delegates to chrono.
    RdtscpTimestampProvider();

    uint64_t now() noexcept override;
    const char* name() const noexcept override;

    bool using_rdtscp() const noexcept { return rdtscp_available_; }
    uint64_t tsc_hz() const noexcept   { return tsc_hz_; }

private:
    bool     rdtscp_available_{false};
    uint64_t tsc_hz_{0};  // TSC ticks per second

    static bool detect_rdtscp() noexcept;
    static uint64_t calibrate_tsc_hz() noexcept;
    static uint64_t read_tsc() noexcept;
};

} // namespace trading
