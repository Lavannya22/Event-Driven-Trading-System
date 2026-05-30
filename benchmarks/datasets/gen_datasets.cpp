// Dataset generator for Phase 3 benchmarks.
//
// Produces:
//   small.csv    100K events
//   medium.csv   1M  events
//   large.bin    10M events (raw Event binary, 64B per record)
//   stress.csv   500K events — high cancel rate + burst traffic
//
// Run: ./gen_datasets <output_dir>

#include "engine/events/Event.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <random>

using namespace trading;

static constexpr uint64_t BASE_PRICE = 10000;
static constexpr uint64_t TICK       = 1;
static constexpr uint32_t SYMBOL     = 1;

// Write a CSV line: timestamp,symbol_id,event_type,price,quantity,order_id,side
static void write_csv_order(std::ofstream& f,
                              uint64_t ts, uint64_t order_id,
                              uint64_t price, uint64_t qty, Side side) {
    f << ts << "," << SYMBOL << "," << static_cast<int>(EventType::NewOrder)
      << "," << price << "," << qty << "," << order_id << ","
      << static_cast<int>(side) << "\n";
}

static void write_csv_cancel(std::ofstream& f,
                               uint64_t ts, uint64_t order_id) {
    f << ts << "," << SYMBOL << "," << static_cast<int>(EventType::CancelOrder)
      << ",0,0," << order_id << ",0\n";
}

// Write a raw Event record to a binary file (64B, Event struct layout).
static void write_binary_event(std::ofstream& f, const Event& e) {
    f.write(reinterpret_cast<const char*>(&e), sizeof(Event));
}

// Generate N alternating resting ask + resting bid events (no crosses).
static void gen_csv(const std::string& path, uint64_t n_events) {
    std::ofstream f(path);
    f << "timestamp,symbol_id,event_type,price,quantity,order_id,side\n";

    uint64_t ts = 1000, oid = 1;
    for (uint64_t i = 0; i < n_events; ++i, ++oid, ts += 100) {
        if (i % 2 == 0)
            write_csv_order(f, ts, oid, BASE_PRICE + (oid % 100) * TICK, 100, Side::Ask);
        else
            write_csv_order(f, ts, oid, BASE_PRICE - (oid % 100) * TICK, 100, Side::Bid);
    }
}

// Stress dataset: interleaved orders + cancels + burst fills.
static void gen_stress_csv(const std::string& path) {
    std::mt19937_64 rng(42);
    std::ofstream f(path);
    f << "timestamp,symbol_id,event_type,price,quantity,order_id,side\n";

    constexpr uint64_t TOTAL  = 500'000;
    constexpr uint64_t WINDOW = 1000;  // keep at most WINDOW resting orders

    uint64_t ts = 1000, oid = 1;
    uint64_t resting[WINDOW]{};
    uint64_t n_resting = 0;

    for (uint64_t i = 0; i < TOTAL; ++i, ts += 50) {
        const bool do_cancel = (n_resting >= WINDOW / 2) &&
                               ((rng() % 4) == 0);
        if (do_cancel && n_resting > 0) {
            const uint64_t idx = rng() % n_resting;
            write_csv_cancel(f, ts, resting[idx]);
            resting[idx] = resting[--n_resting];
        } else {
            const Side   side  = (rng() % 2 == 0) ? Side::Ask : Side::Bid;
            const uint64_t off = (rng() % 50) * TICK;
            const uint64_t px  = (side == Side::Ask)
                                ? BASE_PRICE + off : BASE_PRICE - off;
            write_csv_order(f, ts, oid, px, 50 + (rng() % 50), side);
            if (n_resting < WINDOW) resting[n_resting++] = oid;
            ++oid;
        }
    }
}

// Large binary dataset — raw Event structs, 64B each.
static void gen_binary(const std::string& path) {
    std::ofstream f(path, std::ios::binary);
    constexpr uint64_t N = 10'000'000;
    uint64_t ts = 1000, oid = 1;
    for (uint64_t i = 0; i < N; ++i, ++oid, ts += 10) {
        Event e{};
        e.timestamp = ts;
        e.symbol_id = SYMBOL;
        e.order_id  = oid;
        e.price     = BASE_PRICE + (oid % 100) * TICK;
        e.quantity  = 100;
        e.type      = encode_type(EventType::NewOrder,
                          (i % 2 == 0) ? Side::Ask : Side::Bid);
        write_binary_event(f, e);
    }
    std::printf("large.bin: %llu events written (%llu MB)\n",
        (unsigned long long)N,
        (unsigned long long)(N * sizeof(Event) / 1024 / 1024));
}

int main(int argc, char* argv[]) {
    const std::string dir = (argc > 1) ? argv[1] : ".";

    std::printf("Generating benchmark datasets in: %s\n", dir.c_str());

    gen_csv(dir + "/small.csv",  100'000);
    std::printf("small.csv:  100K events\n");

    gen_csv(dir + "/medium.csv", 1'000'000);
    std::printf("medium.csv: 1M  events\n");

    gen_stress_csv(dir + "/stress.csv");
    std::printf("stress.csv: 500K events (high cancel rate)\n");

    gen_binary(dir + "/large.bin");

    std::printf("Done.\n");
    return 0;
}
