#include "engine/runtime/ThreadAffinityManager.hpp"

#ifdef __linux__
#  include <fstream>
#  include <pthread.h>
#  include <sched.h>
#  include <string>
#endif

namespace trading {

bool ThreadAffinityManager::is_wsl2() noexcept {
#ifdef __linux__
    std::ifstream f("/proc/version");
    std::string line;
    if (std::getline(f, line))
        return line.find("microsoft") != std::string::npos ||
               line.find("Microsoft") != std::string::npos;
#endif
    return false;
}

bool ThreadAffinityManager::pin_thread(std::thread& t, int core_id) noexcept {
#ifdef __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    return pthread_setaffinity_np(t.native_handle(), sizeof(cpu_set_t), &cpuset) == 0;
#else
    (void)t; (void)core_id;
    return false;
#endif
}

bool ThreadAffinityManager::pin_current_thread(int core_id) noexcept {
#ifdef __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0;
#else
    (void)core_id;
    return false;
#endif
}

} // namespace trading
