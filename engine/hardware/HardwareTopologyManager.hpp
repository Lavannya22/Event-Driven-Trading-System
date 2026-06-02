#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace trading {

// Per-node NUMA information.
struct NumaNode {
    int                node_id{-1};
    std::vector<int>   cpus;       // logical CPU ids on this node
    std::size_t        memory_kb{0};
};

// CPU topology snapshot.
struct CpuTopology {
    int  physical_cores{0};     // unique physical cores
    int  logical_cores{0};      // total logical CPUs (with HT)
    bool hyperthreading{false};
    std::vector<int> isolated_cpus;  // from /proc/cmdline isolcpus=
};

// L1/L2/L3 cache sizes (kibibytes).
struct CacheTopology {
    std::size_t l1_kb{0};
    std::size_t l2_kb{0};
    std::size_t l3_kb{0};
};

// Full hardware snapshot.
struct HardwareTopology {
    CpuTopology              cpu;
    std::vector<NumaNode>    numa_nodes;
    CacheTopology            cache;
    bool                     wsl2{false};
    bool                     huge_pages_available{false};
    std::size_t              huge_page_size_kb{0};  // typically 2048
    std::size_t              huge_pages_total{0};
    std::size_t              huge_pages_free{0};
};

// Discovers hardware topology by reading Linux sysfs / procfs.
// All methods return safe defaults on non-Linux platforms and WSL2.
class HardwareTopologyManager {
public:
    // Perform a full topology discovery and return the snapshot.
    static HardwareTopology discover() noexcept;

    // Warn to stderr when HT is enabled on hot-path cores.
    // hot_cores: CPU ids the engine intends to pin to.
    static void warn_if_hyperthreading(const HardwareTopology& topo,
                                       const std::vector<int>& hot_cores) noexcept;

    // Human-readable summary for startup logs / dashboard.
    static std::string format(const HardwareTopology& topo);

private:
    static CpuTopology    discover_cpu()    noexcept;
    static CacheTopology  discover_cache()  noexcept;
    static std::vector<NumaNode> discover_numa() noexcept;
    static bool           detect_wsl2()    noexcept;
    static bool           detect_huge_pages(std::size_t& page_kb,
                                            std::size_t& total,
                                            std::size_t& free_pages) noexcept;
    static std::vector<int> parse_isolated_cpus() noexcept;
};

} // namespace trading
