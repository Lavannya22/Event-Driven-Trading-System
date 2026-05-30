#pragma once

#include <cstdint>
#include "engine/events/Event.hpp"

namespace trading {

// Abstract replay interface. Phase 1 implements SequentialReplayController.
// Keeping this interface separate prevents major architectural refactors later
// when speed-controlled or network-sourced replay is added.
class ReplayController {
public:
    virtual ~ReplayController() = default;

    // Advance to the next event. Writes the event into `out` and returns true.
    // Returns false when the replay source is exhausted.
    virtual bool next(Event& out) noexcept = 0;

    // Rewind to the beginning of the replay source.
    virtual void reset() = 0;

    // True once next() has returned false.
    virtual bool done() const noexcept = 0;

    // Number of events successfully yielded since construction or last reset().
    virtual uint64_t events_replayed() const noexcept = 0;

    // Human-readable name of the replay source (file path, "vector", etc.).
    virtual const char* source_name() const noexcept = 0;
};

} // namespace trading
