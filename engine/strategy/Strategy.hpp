#pragma once

#include <initializer_list>
#include "engine/events/Event.hpp"

namespace trading {

enum class StrategySignal : uint8_t {
    Signal,  // forward event downstream to matching engine
    Noop,    // drop event — do not forward
};

// Abstract strategy interface. Implementations must be:
//   - deterministic: same input always produces same output
//   - non-blocking: on_event must return without waiting
//   - ordering-preserving: no reordering of events
class Strategy {
public:
    virtual ~Strategy() = default;
    virtual StrategySignal on_event(const Event& event) noexcept = 0;
    virtual const char*    name()     const noexcept = 0;
};

// Phase 1 default: forward every event downstream unchanged.
class PassThroughStrategy final : public Strategy {
public:
    StrategySignal on_event(const Event&) noexcept override {
        return StrategySignal::Signal;
    }
    const char* name() const noexcept override { return "PassThrough"; }
};

// Drops every event — useful for testing the NOOP path and benchmarks.
class NullStrategy final : public Strategy {
public:
    StrategySignal on_event(const Event&) noexcept override {
        return StrategySignal::Noop;
    }
    const char* name() const noexcept override { return "Null"; }
};

// Forwards only the event types in the allowed set; NOOPs everything else.
// Allows filtering e.g. MarketUpdate events before they reach the matcher.
class EventTypeFilterStrategy final : public Strategy {
public:
    explicit EventTypeFilterStrategy(std::initializer_list<EventType> allowed) {
        for (EventType t : allowed) {
            const auto idx = static_cast<uint8_t>(t);
            if (idx < kSize) allowed_[idx] = true;
        }
    }

    StrategySignal on_event(const Event& e) noexcept override {
        const auto idx = static_cast<uint8_t>(event_type(e));
        if (idx < kSize && allowed_[idx]) return StrategySignal::Signal;
        return StrategySignal::Noop;
    }

    const char* name() const noexcept override { return "EventTypeFilter"; }

private:
    static constexpr uint8_t kSize = 8;  // covers EventType values 1–5 with headroom
    bool allowed_[kSize]{};
};

} // namespace trading
