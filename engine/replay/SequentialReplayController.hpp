#pragma once

#include <string>
#include <fstream>
#include <cstdint>
#include "engine/replay/ReplayController.hpp"

namespace trading {

// Reads events sequentially from a CSV file and replays them in file order.
//
// CSV format (header line optional, comment lines start with #):
//   timestamp,symbol_id,event_type,price,quantity,order_id[,side]
//
// event_type maps to EventType enum values (1–5).
// side: 0 = Bid (default), 1 = Ask.
//
// Out-of-order timestamps are detected and flagged but do NOT reorder events —
// file order is the canonical deterministic sequence.
class SequentialReplayController final : public ReplayController {
public:
    explicit SequentialReplayController(std::string csv_path);

    bool     next(Event& out)          noexcept override;
    void     reset()                            override;
    bool     done()              const noexcept override { return done_; }
    uint64_t events_replayed()   const noexcept override { return events_replayed_; }
    const char* source_name()    const noexcept override { return path_.c_str(); }

    // True if any out-of-order timestamps were detected during replay.
    bool had_out_of_order_timestamps() const noexcept { return out_of_order_count_ > 0; }
    uint64_t out_of_order_count()      const noexcept { return out_of_order_count_; }

private:
    void open_file();
    bool parse_line(const std::string& line, Event& out) const noexcept;

    std::string   path_;
    std::ifstream file_;
    uint64_t      events_replayed_{0};
    uint64_t      last_timestamp_{0};
    uint64_t      out_of_order_count_{0};
    bool          done_{false};
};

} // namespace trading
