#pragma once

#include "engine/memory/StartupAllocator.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace trading {

struct ValidationResult {
    bool        passed{true};
    std::string category;
    std::string message;
};

struct ValidationReport {
    std::vector<ValidationResult> results;
    bool all_passed() const noexcept {
        for (const auto& r : results)
            if (!r.passed) return false;
        return true;
    }
};

// Validates engine configuration before the hot path begins.
// Critical failures abort startup. Non-critical failures emit warnings.
//
// WSL2 detection: NUMA and thread-affinity checks are skipped on WSL2
// because the kernel virtualisation layer does not expose real NUMA topology.
class StartupValidator {
public:
    // Run all validation categories against the allocator and its config.
    ValidationReport validate(const StartupAllocator& alloc,
                              const StartupAllocator::Config& cfg);

private:
    void check_pools(const StartupAllocator& alloc,
                     const StartupAllocator::Config& cfg,
                     ValidationReport& report);
    void check_timing(ValidationReport& report);
    void check_numa(ValidationReport& report);

    static bool is_wsl2() noexcept;

    static void pass(ValidationReport& r, std::string_view category,
                     std::string_view msg);
    static void fail(ValidationReport& r, std::string_view category,
                     std::string_view msg);
};

} // namespace trading
