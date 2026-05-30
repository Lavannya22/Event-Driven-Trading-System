#pragma once

#include <thread>

namespace trading {

// Sets CPU affinity for engine threads.
//
// Linux (bare-metal): failure aborts startup — mismatched affinity causes
//   unpredictable scheduling jitter that invalidates latency measurements.
//
// WSL2: failure emits a warning and continues — the WSL2 kernel may reject
//   affinity calls even when running on a multi-core host.
class ThreadAffinityManager {
public:
    // Pins the native thread to core_id.
    // Returns true on success. On failure the caller decides to abort or warn.
    bool pin_thread(std::thread& t, int core_id) noexcept;

    // Pins the calling thread to core_id (useful when the thread function
    // sets its own affinity at startup).
    bool pin_current_thread(int core_id) noexcept;

    // True when running inside WSL2 (affinity failures are non-fatal there).
    static bool is_wsl2() noexcept;
};

} // namespace trading
