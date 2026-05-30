#include "engine/replay/SequentialReplayController.hpp"
#include "engine/events/Event.hpp"
#include <cinttypes>
#include <sstream>
#include <stdexcept>

namespace trading {

SequentialReplayController::SequentialReplayController(std::string csv_path)
    : path_(std::move(csv_path))
{
    open_file();
}

void SequentialReplayController::open_file() {
    file_.close();
    file_.clear();
    file_.open(path_);
    if (!file_.is_open())
        throw std::runtime_error("SequentialReplayController: cannot open: " + path_);
}

void SequentialReplayController::reset() {
    open_file();
    events_replayed_    = 0;
    last_timestamp_     = 0;
    out_of_order_count_ = 0;
    done_               = false;
}

bool SequentialReplayController::next(Event& out) noexcept {
    if (done_) return false;

    std::string line;
    while (std::getline(file_, line)) {
        // Strip trailing carriage return (Windows line endings)
        if (!line.empty() && line.back() == '\r') line.pop_back();

        // Skip blanks and comments
        if (line.empty() || line[0] == '#') continue;

        // Skip header row (starts with a non-digit)
        if (line[0] < '0' || line[0] > '9') continue;

        if (!parse_line(line, out)) continue;  // malformed — skip silently

        // Detect out-of-order timestamps
        if (out.timestamp < last_timestamp_) ++out_of_order_count_;
        last_timestamp_ = out.timestamp;

        ++events_replayed_;
        return true;
    }

    done_ = true;
    return false;
}

bool SequentialReplayController::parse_line(const std::string& line,
                                             Event& out) const noexcept {
    // Expected fields: timestamp,symbol_id,event_type,price,quantity,order_id[,side]
    uint64_t timestamp{}, price{}, quantity{}, order_id{};
    uint32_t symbol_id{}, event_type_val{};
    uint32_t side_val{0};  // default Bid

    // Fast path: parse with sscanf — no allocations, deterministic cost
    int fields = std::sscanf(line.c_str(),
                              "%" SCNu64 ",%u,%u,%" SCNu64 ",%" SCNu64 ",%" SCNu64 ",%u",
                              &timestamp, &symbol_id, &event_type_val,
                              &price, &quantity, &order_id, &side_val);

    if (fields < 6) return false;

    const auto etype = static_cast<EventType>(event_type_val);
    const auto side  = static_cast<Side>(side_val & 0x1u);

    // Validate event type range
    if (event_type_val < 1 || event_type_val > 5) return false;

    out          = Event{};
    out.timestamp = timestamp;
    out.symbol_id = symbol_id;
    out.type      = encode_type(etype, side);
    out.price     = price;
    out.quantity  = quantity;
    out.order_id  = order_id;
    return true;
}

} // namespace trading
