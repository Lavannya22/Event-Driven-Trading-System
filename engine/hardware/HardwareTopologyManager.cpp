#include "engine/hardware/HardwareTopologyManager.hpp"
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>

#ifdef __linux__
#  include <fstream>
#  include <dirent.h>
#  include <unistd.h>
#endif

namespace trading {

// ── WSL2 detection ────────────────────────────────────────────────────────────

bool HardwareTopologyManager::detect_wsl2() noexcept {
#ifdef __linux__
    std::ifstream f("/proc/version");
    std::string line;
    if (std::getline(f, line))
        return line.find("microsoft") != std::string::npos ||
               line.find("Microsoft") != std::string::npos;
#endif
    return false;
}

// ── CPU topology ──────────────────────────────────────────────────────────────

CpuTopology HardwareTopologyManager::discover_cpu() noexcept {
    CpuTopology topo;
#ifdef __linux__
    // Count logical CPUs
    std::ifstream f("/sys/devices/system/cpu/present");
    std::string range;
    if (std::getline(f, range)) {
        // Parse "0-N" or "0,1,2,..."
        int last = 0;
        std::sscanf(range.c_str(), "%*d-%d", &last);
        topo.logical_cores = last + 1;
        if (topo.logical_cores <= 0) topo.logical_cores = 1;
    }

    // Count physical cores via topology/core_id
    std::ifstream info("/proc/cpuinfo");
    std::string line;
    int max_core_id = -1;
    while (std::getline(info, line)) {
        if (line.compare(0, 7, "cpu MHz") == 0) continue;
        if (line.compare(0, 7, "core id") == 0) {
            int id = -1;
            std::sscanf(line.c_str(), "core id : %d", &id);
            if (id > max_core_id) max_core_id = id;
        }
    }
    topo.physical_cores = (max_core_id >= 0) ? max_core_id + 1
                                              : topo.logical_cores;
    topo.hyperthreading = (topo.logical_cores > topo.physical_cores);
    topo.isolated_cpus  = parse_isolated_cpus();
#else
    topo.logical_cores  = 1;
    topo.physical_cores = 1;
#endif
    return topo;
}

// ── Cache topology ────────────────────────────────────────────────────────────

CacheTopology HardwareTopologyManager::discover_cache() noexcept {
    CacheTopology cache;
#ifdef __linux__
    // Walk /sys/devices/system/cpu/cpu0/cache/index*
    for (int idx = 0; idx < 8; ++idx) {
        char path[128];
        std::snprintf(path, sizeof(path),
            "/sys/devices/system/cpu/cpu0/cache/index%d/level", idx);
        std::ifstream fl(path);
        if (!fl.is_open()) break;

        int level = 0;
        fl >> level;

        char sz_path[128];
        std::snprintf(sz_path, sizeof(sz_path),
            "/sys/devices/system/cpu/cpu0/cache/index%d/size", idx);
        std::ifstream fs(sz_path);
        std::string sz;
        std::size_t kb = 0;
        if (std::getline(fs, sz)) {
            // Format: "32K" or "512K" or "8192K"
            std::sscanf(sz.c_str(), "%zuK", &kb);
        }

        if (level == 1 && cache.l1_kb == 0) cache.l1_kb = kb;
        else if (level == 2 && cache.l2_kb == 0) cache.l2_kb = kb;
        else if (level == 3 && cache.l3_kb == 0) cache.l3_kb = kb;
    }
#endif
    return cache;
}

// ── NUMA topology ─────────────────────────────────────────────────────────────

std::vector<NumaNode> HardwareTopologyManager::discover_numa() noexcept {
    std::vector<NumaNode> nodes;
#ifdef __linux__
    // /sys/devices/system/node/nodeN
    for (int n = 0; n < 8; ++n) {
        char path[128];
        std::snprintf(path, sizeof(path),
            "/sys/devices/system/node/node%d", n);
        std::ifstream test(std::string(path) + "/cpulist");
        if (!test.is_open()) break;

        NumaNode node;
        node.node_id = n;

        // Parse cpulist (e.g. "0-3,8-11")
        std::string cpulist;
        std::getline(test, cpulist);
        std::istringstream ss(cpulist);
        std::string token;
        while (std::getline(ss, token, ',')) {
            int a = -1, b = -1;
            if (std::sscanf(token.c_str(), "%d-%d", &a, &b) == 2) {
                for (int c = a; c <= b; ++c) node.cpus.push_back(c);
            } else if (std::sscanf(token.c_str(), "%d", &a) == 1) {
                node.cpus.push_back(a);
            }
        }

        // Memory size
        char mem_path[160];
        std::snprintf(mem_path, sizeof(mem_path),
            "/sys/devices/system/node/node%d/meminfo", n);
        std::ifstream mf(mem_path);
        std::string mline;
        while (std::getline(mf, mline)) {
            if (mline.find("MemTotal") != std::string::npos) {
                std::sscanf(mline.c_str(), "%*s %*s %zu kB", &node.memory_kb);
                break;
            }
        }

        nodes.push_back(std::move(node));
    }
#endif
    if (nodes.empty()) {
        // Single-node fallback
        NumaNode n0; n0.node_id = 0;
        nodes.push_back(n0);
    }
    return nodes;
}

// ── Isolated CPUs ─────────────────────────────────────────────────────────────

std::vector<int> HardwareTopologyManager::parse_isolated_cpus() noexcept {
    std::vector<int> cpus;
#ifdef __linux__
    std::ifstream f("/proc/cmdline");
    std::string line;
    if (!std::getline(f, line)) return cpus;

    const auto pos = line.find("isolcpus=");
    if (pos == std::string::npos) return cpus;

    const std::string val = line.substr(pos + 9,
        line.find(' ', pos + 9) - (pos + 9));
    std::istringstream ss(val);
    std::string token;
    while (std::getline(ss, token, ',')) {
        int a = -1, b = -1;
        if (std::sscanf(token.c_str(), "%d-%d", &a, &b) == 2) {
            for (int c = a; c <= b; ++c) cpus.push_back(c);
        } else if (std::sscanf(token.c_str(), "%d", &a) == 1) {
            cpus.push_back(a);
        }
    }
#endif
    return cpus;
}

// ── Huge pages ────────────────────────────────────────────────────────────────

bool HardwareTopologyManager::detect_huge_pages(std::size_t& page_kb,
                                                 std::size_t& total,
                                                 std::size_t& free_pages) noexcept {
    page_kb = 0; total = 0; free_pages = 0;
#ifdef __linux__
    std::ifstream f("/proc/meminfo");
    std::string line;
    while (std::getline(f, line)) {
        if (line.compare(0, 14, "Hugepagesize: ") == 0 ||
            line.compare(0, 14, "Hugepagesize:") == 0) {
            std::sscanf(line.c_str(), "Hugepagesize: %zu kB", &page_kb);
        }
        if (line.compare(0, 13, "HugePages_Total") == 0) {
            std::sscanf(line.c_str(), "HugePages_Total: %zu", &total);
        }
        if (line.compare(0, 12, "HugePages_Free") == 0) {
            std::sscanf(line.c_str(), "HugePages_Free: %zu", &free_pages);
        }
    }
    return (total > 0 && page_kb > 0);
#else
    return false;
#endif
}

// ── discover ──────────────────────────────────────────────────────────────────

HardwareTopology HardwareTopologyManager::discover() noexcept {
    HardwareTopology topo;
    topo.wsl2       = detect_wsl2();
    topo.cpu        = discover_cpu();
    topo.cache      = discover_cache();
    topo.numa_nodes = discover_numa();
    topo.huge_pages_available =
        detect_huge_pages(topo.huge_page_size_kb,
                          topo.huge_pages_total,
                          topo.huge_pages_free);
    return topo;
}

// ── warn_if_hyperthreading ────────────────────────────────────────────────────

void HardwareTopologyManager::warn_if_hyperthreading(
    const HardwareTopology& topo,
    const std::vector<int>& hot_cores) noexcept {

    if (!topo.cpu.hyperthreading) return;
    if (hot_cores.empty()) return;

    std::cerr << "[topology] WARNING: Hyperthreading is enabled. "
              << "Hot-path cores may share physical cores with OS threads, "
              << "causing scheduling jitter. Disable HT in BIOS for "
              << "authoritative latency measurements.\n";
    (void)hot_cores;
}

// ── format ────────────────────────────────────────────────────────────────────

std::string HardwareTopologyManager::format(const HardwareTopology& topo) {
    std::ostringstream ss;
    ss << "Hardware Topology\n"
       << "  Platform:       " << (topo.wsl2 ? "WSL2 (fallbacks active)" : "Linux bare-metal") << "\n"
       << "  Physical cores: " << topo.cpu.physical_cores << "\n"
       << "  Logical CPUs:   " << topo.cpu.logical_cores  << "\n"
       << "  Hyperthreading: " << (topo.cpu.hyperthreading ? "ENABLED (not ideal)" : "disabled") << "\n"
       << "  Isolated CPUs:  ";
    if (topo.cpu.isolated_cpus.empty()) ss << "none";
    else for (int c : topo.cpu.isolated_cpus) ss << c << " ";
    ss << "\n"
       << "  Cache L1:       " << topo.cache.l1_kb << " KB\n"
       << "  Cache L2:       " << topo.cache.l2_kb << " KB\n"
       << "  Cache L3:       " << topo.cache.l3_kb << " KB\n"
       << "  NUMA nodes:     " << topo.numa_nodes.size() << "\n"
       << "  Huge pages:     ";
    if (topo.huge_pages_available)
        ss << topo.huge_pages_free << "/" << topo.huge_pages_total
           << " free (" << topo.huge_page_size_kb << " KB each)";
    else
        ss << "not configured";
    ss << "\n";
    return ss.str();
}

} // namespace trading
