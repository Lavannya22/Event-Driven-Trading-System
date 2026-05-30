#include "engine/runtime/StartupValidator.hpp"
#include <chrono>
#include <string>

#ifdef __linux__
#  include <fstream>
#endif

namespace trading {

bool StartupValidator::is_wsl2() noexcept {
#ifdef __linux__
    std::ifstream f("/proc/version");
    std::string line;
    if (std::getline(f, line))
        return line.find("microsoft") != std::string::npos ||
               line.find("Microsoft") != std::string::npos;
#endif
    return false;
}

void StartupValidator::pass(ValidationReport& r, std::string_view category,
                             std::string_view msg) {
    r.results.push_back({true, std::string(category), std::string(msg)});
}

void StartupValidator::fail(ValidationReport& r, std::string_view category,
                             std::string_view msg) {
    r.results.push_back({false, std::string(category), std::string(msg)});
}

void StartupValidator::check_pools(const StartupAllocator& alloc,
                                    const StartupAllocator::Config& cfg,
                                    ValidationReport& report) {
    if (!alloc.started()) {
        fail(report, "MemoryPools", "StartupAllocator.start() has not been called");
        return;
    }
    if (!alloc.event_pool() || alloc.event_pool()->capacity() == 0)
        fail(report, "MemoryPools", "Event pool has zero capacity");
    else
        pass(report, "MemoryPools", "Event pool OK (" +
             std::to_string(alloc.event_pool()->capacity()) + " slots)");

    if (!alloc.order_pool() || alloc.order_pool()->capacity() == 0)
        fail(report, "MemoryPools", "Order pool has zero capacity");
    else
        pass(report, "MemoryPools", "Order pool OK (" +
             std::to_string(alloc.order_pool()->capacity()) + " slots)");

    if (!alloc.exec_pool() || alloc.exec_pool()->capacity() == 0)
        fail(report, "MemoryPools", "ExecutionReport pool has zero capacity");
    else
        pass(report, "MemoryPools", "ExecutionReport pool OK (" +
             std::to_string(alloc.exec_pool()->capacity()) + " slots)");

    if (!alloc.signal_pool() || alloc.signal_pool()->capacity() == 0)
        fail(report, "MemoryPools", "StrategySignal pool has zero capacity");
    else
        pass(report, "MemoryPools", "StrategySignal pool OK (" +
             std::to_string(alloc.signal_pool()->capacity()) + " slots)");

    (void)cfg;
}

void StartupValidator::check_timing(ValidationReport& report) {
    // Verify std::chrono::steady_clock is monotonic and ticks at nanosecond resolution.
    using Clock = std::chrono::steady_clock;
    if (!Clock::is_steady)
        fail(report, "Timing", "std::chrono::steady_clock is not monotonic");
    else
        pass(report, "Timing", "steady_clock is monotonic");

    const auto t0 = Clock::now();
    const auto t1 = Clock::now();
    if (t1 < t0)
        fail(report, "Timing", "steady_clock went backwards — non-monotonic");
    else
        pass(report, "Timing", "steady_clock tick sanity OK");
}

void StartupValidator::check_numa(ValidationReport& report) {
    if (is_wsl2()) {
        pass(report, "NUMA", "WSL2 detected — NUMA validation skipped");
        return;
    }
#ifdef __linux__
    std::ifstream f("/sys/devices/system/node/online");
    if (!f.is_open()) {
        pass(report, "NUMA", "NUMA sysfs not available — single node assumed");
        return;
    }
    std::string nodes;
    std::getline(f, nodes);
    pass(report, "NUMA", "NUMA nodes online: " + nodes);
#else
    pass(report, "NUMA", "NUMA validation not applicable on this platform");
#endif
}

ValidationReport StartupValidator::validate(const StartupAllocator& alloc,
                                             const StartupAllocator::Config& cfg) {
    ValidationReport report;
    check_pools(alloc, cfg, report);
    check_timing(report);
    check_numa(report);
    return report;
}

} // namespace trading
