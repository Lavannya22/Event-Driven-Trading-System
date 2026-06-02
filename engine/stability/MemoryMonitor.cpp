#include "engine/stability/MemoryMonitor.hpp"
#include <chrono>

#ifdef __linux__
#  include <cstdio>
#  include <fstream>
#  include <string>
#endif

namespace trading {

uint64_t MemoryMonitor::now_ns() noexcept {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count());
}

std::size_t MemoryMonitor::current_rss_mb() noexcept {
#ifdef __linux__
    std::ifstream f("/proc/self/status");
    std::string line;
    while (std::getline(f, line)) {
        if (line.compare(0, 6, "VmRSS:") == 0) {
            std::size_t kb = 0;
            std::sscanf(line.c_str(), "VmRSS: %zu kB", &kb);
            return kb / 1024;
        }
    }
#endif
    return 0;
}

void MemoryMonitor::set_baseline() noexcept {
    baseline_mb_      = current_rss_mb();
    baseline_time_ns_ = now_ns();
    current_mb_       = baseline_mb_;
    current_time_ns_  = baseline_time_ns_;
    if (current_mb_ > peak_mb_) peak_mb_ = current_mb_;
}

void MemoryMonitor::record() noexcept {
    current_mb_      = current_rss_mb();
    current_time_ns_ = now_ns();
    if (current_mb_ > peak_mb_) peak_mb_ = current_mb_;
}

std::size_t MemoryMonitor::growth_mb() const noexcept {
    return current_mb_ >= baseline_mb_ ? current_mb_ - baseline_mb_ : 0;
}

double MemoryMonitor::growth_rate_mb_per_hour() const noexcept {
    if (baseline_time_ns_ == 0 || current_time_ns_ <= baseline_time_ns_) return 0.0;
    const double elapsed_h =
        static_cast<double>(current_time_ns_ - baseline_time_ns_) / 3.6e12;
    if (elapsed_h < 1e-9) return 0.0;
    return static_cast<double>(growth_mb()) / elapsed_h;
}

double MemoryMonitor::projected_24h_growth_mb() const noexcept {
    return growth_rate_mb_per_hour() * 24.0;
}

void MemoryMonitor::reset() noexcept {
    current_mb_       = 0;
    peak_mb_          = 0;
    baseline_mb_      = 0;
    baseline_time_ns_ = 0;
    current_time_ns_  = 0;
}

} // namespace trading
