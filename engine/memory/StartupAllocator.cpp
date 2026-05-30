#include "engine/memory/StartupAllocator.hpp"

#ifdef __linux__
#  include <sys/mman.h>
#endif

namespace trading {

StartupReport StartupAllocator::start(const Config& cfg) {
    StartupReport report;

    event_pool_  = std::make_unique<ObjectPool<Event>>(cfg.event_pool_size);
    order_pool_  = std::make_unique<ObjectPool<Order>>(cfg.order_pool_size);
    exec_pool_   = std::make_unique<ObjectPool<ExecutionReport>>(cfg.execution_pool_size);
    signal_pool_ = std::make_unique<ObjectPool<SignalRecord>>(cfg.signal_pool_size);

    report.event_pool_capacity     = cfg.event_pool_size;
    report.order_pool_capacity     = cfg.order_pool_size;
    report.execution_pool_capacity = cfg.execution_pool_size;
    report.signal_pool_capacity    = cfg.signal_pool_size;

#ifdef __linux__
    if (cfg.attempt_mlockall) {
        report.mlockall_success =
            (mlockall(MCL_CURRENT | MCL_FUTURE) == 0);
    }
#else
    report.mlockall_success = false;
#endif

    started_ = true;
    return report;
}

} // namespace trading
