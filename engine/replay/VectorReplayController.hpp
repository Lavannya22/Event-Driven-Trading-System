#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include "engine/replay/ReplayController.hpp"

namespace trading {

// In-memory replay from a pre-built event vector.
// Used in unit and integration tests to avoid file I/O.
class VectorReplayController final : public ReplayController {
public:
    explicit VectorReplayController(std::vector<Event> events,
                                     std::string name = "vector")
        : events_(std::move(events))
        , name_(std::move(name))
    {}

    bool next(Event& out) noexcept override {
        if (cursor_ >= events_.size()) return false;
        out = events_[cursor_++];
        ++events_replayed_;
        return true;
    }

    void reset() override {
        cursor_          = 0;
        events_replayed_ = 0;
    }

    bool     done()            const noexcept override { return cursor_ >= events_.size(); }
    uint64_t events_replayed() const noexcept override { return events_replayed_; }
    const char* source_name()  const noexcept override { return name_.c_str(); }

private:
    std::vector<Event> events_;
    std::string        name_;
    std::size_t        cursor_{0};
    uint64_t           events_replayed_{0};
};

} // namespace trading
