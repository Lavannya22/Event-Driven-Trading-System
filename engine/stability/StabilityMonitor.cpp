#include "engine/stability/StabilityMonitor.hpp"
#include <sstream>

namespace trading {

StabilityMonitor::StabilityMonitor(StabilityThresholds thresholds)
    : thresholds_(thresholds)
{}

void StabilityMonitor::record_latency_p99(uint64_t p99_ns) noexcept {
    current_p99_ns_ = p99_ns;
}

void StabilityMonitor::record_throughput(double eps) noexcept {
    current_throughput_ = eps;
}

void StabilityMonitor::record_memory_mb(std::size_t rss_mb) noexcept {
    current_memory_mb_ = rss_mb;
}

void StabilityMonitor::record_queue_depth_pct(double pct) noexcept {
    current_queue_pct_ = pct;
}

void StabilityMonitor::record_pool_exhaustions(std::size_t total) noexcept {
    total_pool_exhaustions_ = total;
}

void StabilityMonitor::set_baseline() noexcept {
    baseline_p99_ns_     = current_p99_ns_;
    baseline_throughput_ = current_throughput_;
    baseline_memory_mb_  = current_memory_mb_;
}

double StabilityMonitor::latency_drift_pct() const noexcept {
    if (baseline_p99_ns_ == 0) return 0.0;
    return (static_cast<double>(current_p99_ns_) -
            static_cast<double>(baseline_p99_ns_)) /
           static_cast<double>(baseline_p99_ns_) * 100.0;
}

double StabilityMonitor::throughput_drift_pct() const noexcept {
    if (baseline_throughput_ < 1e-9) return 0.0;
    return (current_throughput_ - baseline_throughput_) /
           baseline_throughput_ * 100.0;
}

std::size_t StabilityMonitor::memory_growth_mb() const noexcept {
    return current_memory_mb_ >= baseline_memory_mb_
               ? current_memory_mb_ - baseline_memory_mb_
               : 0;
}

std::vector<StabilityAlert> StabilityMonitor::check() const {
    std::vector<StabilityAlert> alerts;

    const double lat = latency_drift_pct();
    if (lat > thresholds_.max_latency_drift_pct) {
        std::ostringstream msg;
        msg << "Latency drift " << lat << "% > threshold "
            << thresholds_.max_latency_drift_pct << "%";
        alerts.push_back({StabilityAlert::Kind::LatencyDrift,
                          lat, thresholds_.max_latency_drift_pct, msg.str()});
    }

    const double tp = throughput_drift_pct();
    if (tp < -static_cast<double>(thresholds_.max_throughput_drift_pct)) {
        const double drop = -tp;
        std::ostringstream msg;
        msg << "Throughput dropped " << drop << "% > threshold "
            << thresholds_.max_throughput_drift_pct << "%";
        alerts.push_back({StabilityAlert::Kind::ThroughputDrop,
                          drop, thresholds_.max_throughput_drift_pct, msg.str()});
    }

    const std::size_t mem = memory_growth_mb();
    if (mem > thresholds_.max_memory_growth_mb) {
        std::ostringstream msg;
        msg << "Memory growth " << mem << " MB > threshold "
            << thresholds_.max_memory_growth_mb << " MB";
        alerts.push_back({StabilityAlert::Kind::MemoryGrowth,
                          static_cast<double>(mem),
                          static_cast<double>(thresholds_.max_memory_growth_mb),
                          msg.str()});
    }

    if (current_queue_pct_ >
            static_cast<double>(thresholds_.max_queue_depth_pct)) {
        std::ostringstream msg;
        msg << "Queue depth " << current_queue_pct_ << "% > threshold "
            << thresholds_.max_queue_depth_pct << "%";
        alerts.push_back({StabilityAlert::Kind::QueueDepth,
                          current_queue_pct_,
                          static_cast<double>(thresholds_.max_queue_depth_pct),
                          msg.str()});
    }

    if (total_pool_exhaustions_ > thresholds_.max_pool_exhaustions) {
        std::ostringstream msg;
        msg << total_pool_exhaustions_ << " pool exhaustion(s) detected";
        alerts.push_back({StabilityAlert::Kind::PoolExhaustion,
                          static_cast<double>(total_pool_exhaustions_),
                          static_cast<double>(thresholds_.max_pool_exhaustions),
                          msg.str()});
    }

    return alerts;
}

bool StabilityMonitor::is_stable() const {
    return check().empty();
}

void StabilityMonitor::reset() noexcept {
    baseline_p99_ns_        = 0;
    baseline_throughput_    = 0.0;
    baseline_memory_mb_     = 0;
    current_p99_ns_         = 0;
    current_throughput_     = 0.0;
    current_memory_mb_      = 0;
    current_queue_pct_      = 0.0;
    total_pool_exhaustions_ = 0;
}

} // namespace trading
