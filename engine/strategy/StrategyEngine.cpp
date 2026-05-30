#include "engine/strategy/StrategyEngine.hpp"
#include <stdexcept>

namespace trading {

StrategyEngine::StrategyEngine(std::unique_ptr<Strategy> strategy)
    : strategy_(std::move(strategy))
{
    if (!strategy_)
        throw std::invalid_argument("StrategyEngine: strategy must not be null");
}

StrategySignal StrategyEngine::process(const Event& event) noexcept {
    const StrategySignal sig = strategy_->on_event(event);
    if (sig == StrategySignal::Signal) ++signal_count_;
    else                               ++noop_count_;
    return sig;
}

void StrategyEngine::set_strategy(std::unique_ptr<Strategy> strategy) {
    if (!strategy)
        throw std::invalid_argument("StrategyEngine: strategy must not be null");
    strategy_ = std::move(strategy);
}

} // namespace trading
