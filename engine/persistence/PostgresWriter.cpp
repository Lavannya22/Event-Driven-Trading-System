#include "engine/persistence/PostgresWriter.hpp"

#ifdef WITH_POSTGRES
#  include <libpq-fe.h>
#endif

#include <atomic>
#include <chrono>
#include <cinttypes>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <random>
#include <string>
#include <thread>

namespace trading {

// ── UUID v4 ───────────────────────────────────────────────────────────────────
static std::string gen_uuid_v4() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> d;
    const uint32_t a  = d(gen);
    const uint32_t b  = d(gen);
    const uint32_t c  = (d(gen) & 0x0FFFu) | 0x4000u; // version 4
    const uint32_t dv = (d(gen) & 0x3FFFu) | 0x8000u; // variant 10xx
    const uint32_t e1 = d(gen);
    const uint16_t e2 = static_cast<uint16_t>(d(gen));
    char buf[37];
    std::snprintf(buf, sizeof(buf), "%08x-%04x-%04x-%04x-%08x%04x",
                  a, b & 0xFFFFu, c, dv, e1, static_cast<unsigned>(e2));
    return buf;
}

// ── Internal async command ────────────────────────────────────────────────────
struct Cmd {
    enum class Kind { Trade, BeginRun, EndRun, Flush, Shutdown };
    Kind        kind{Kind::Shutdown};
    std::string run_id;
    std::string replay_file;  // BeginRun
    uint64_t    timestamp{0};
    uint32_t    symbol_id{0};
    uint64_t    order_id{0};
    uint64_t    price{0};
    uint64_t    quantity{0};
    uint8_t     side{0};
    uint64_t    event_count{0}; // EndRun
    std::shared_ptr<std::atomic_bool> flush_done; // Flush acknowledgement
};

// ── Impl ──────────────────────────────────────────────────────────────────────
struct PostgresWriter::Impl {
    Config cfg_;
    bool   connected_{false};

#ifdef WITH_POSTGRES
    PGconn* conn_{nullptr};
#endif

    std::mutex              mtx_;
    std::condition_variable cv_;
    std::queue<Cmd>         queue_;
    std::thread             thread_;

    explicit Impl(Config cfg) : cfg_(std::move(cfg)) {
        thread_ = std::thread([this] { writer_loop(); });
    }

    ~Impl() {
        Cmd stop; stop.kind = Cmd::Kind::Shutdown; enqueue(std::move(stop));
        if (thread_.joinable()) thread_.join();
#ifdef WITH_POSTGRES
        if (conn_) { PQfinish(conn_); conn_ = nullptr; }
#endif
    }

    void enqueue(Cmd cmd) {
        {
            std::lock_guard lk(mtx_);
            queue_.push(std::move(cmd));
        }
        cv_.notify_one();
    }

    void writer_loop() {
#ifdef WITH_POSTGRES
        conn_ = PQconnectdb(cfg_.conn_str.c_str());
        if (PQstatus(conn_) != CONNECTION_OK) {
            std::cerr << "[persistence] PostgreSQL connection failed: "
                      << PQerrorMessage(conn_) << '\n';
            PQfinish(conn_);
            conn_ = nullptr;
        } else {
            connected_ = true;
            std::cout << "[persistence] Connected to PostgreSQL.\n";
        }
#endif
        while (true) {
            Cmd cmd;
            {
                std::unique_lock lk(mtx_);
                cv_.wait(lk, [this] { return !queue_.empty(); });
                cmd = std::move(queue_.front());
                queue_.pop();
            }
            switch (cmd.kind) {
                case Cmd::Kind::Shutdown: return;
                case Cmd::Kind::Flush:
                    if (cmd.flush_done)
                        cmd.flush_done->store(true, std::memory_order_release);
                    break;
                case Cmd::Kind::BeginRun: exec_begin_run(cmd); break;
                case Cmd::Kind::EndRun:   exec_end_run(cmd);   break;
                case Cmd::Kind::Trade:    exec_trade(cmd);     break;
            }
        }
    }

    void exec_begin_run(const Cmd& cmd) {
#ifdef WITH_POSTGRES
        if (!conn_) return;
        const char* params[2] = {cmd.run_id.c_str(), cmd.replay_file.c_str()};
        PGresult* res = PQexecParams(conn_,
            "INSERT INTO replay_runs(run_id, started_at, replay_file)"
            " VALUES($1::uuid, now(), $2)",
            2, nullptr, params, nullptr, nullptr, 0);
        if (PQresultStatus(res) != PGRES_COMMAND_OK)
            std::cerr << "[persistence] begin_run: " << PQerrorMessage(conn_);
        PQclear(res);
#else
        (void)cmd;
#endif
    }

    void exec_end_run(const Cmd& cmd) {
#ifdef WITH_POSTGRES
        if (!conn_) return;
        char cnt[24];
        std::snprintf(cnt, sizeof(cnt), "%" PRIu64, cmd.event_count);
        const char* params[2] = {cnt, cmd.run_id.c_str()};
        PGresult* res = PQexecParams(conn_,
            "UPDATE replay_runs SET ended_at = now(), event_count = $1"
            " WHERE run_id = $2::uuid",
            2, nullptr, params, nullptr, nullptr, 0);
        if (PQresultStatus(res) != PGRES_COMMAND_OK)
            std::cerr << "[persistence] end_run: " << PQerrorMessage(conn_);
        PQclear(res);
#else
        (void)cmd;
#endif
    }

    void exec_trade(const Cmd& cmd) {
#ifdef WITH_POSTGRES
        if (!conn_) return;
        char ts[24], sym[12], oid[24], p[24], q[24], s[4];
        std::snprintf(ts,  sizeof(ts),  "%" PRIu64, cmd.timestamp);
        std::snprintf(sym, sizeof(sym), "%u",       cmd.symbol_id);
        std::snprintf(oid, sizeof(oid), "%" PRIu64, cmd.order_id);
        std::snprintf(p,   sizeof(p),   "%" PRIu64, cmd.price);
        std::snprintf(q,   sizeof(q),   "%" PRIu64, cmd.quantity);
        std::snprintf(s,   sizeof(s),   "%d",       static_cast<int>(cmd.side));
        const char* params[7] = {ts, sym, oid, p, q, s, cmd.run_id.c_str()};
        PGresult* res = PQexecParams(conn_,
            "INSERT INTO executions"
            "(timestamp, symbol_id, order_id, price, quantity, side, run_id)"
            " VALUES($1,$2,$3,$4,$5,$6,$7::uuid)",
            7, nullptr, params, nullptr, nullptr, 0);
        if (PQresultStatus(res) != PGRES_COMMAND_OK)
            std::cerr << "[persistence] write_trade: " << PQerrorMessage(conn_);
        PQclear(res);
#else
        (void)cmd;
#endif
    }
};

// ── Public API ────────────────────────────────────────────────────────────────
PostgresWriter::PostgresWriter(Config cfg)
    : impl_(std::make_unique<Impl>(std::move(cfg))) {}

PostgresWriter::~PostgresWriter() = default;

bool PostgresWriter::connected() const noexcept { return impl_->connected_; }

std::string PostgresWriter::begin_run(const std::string& replay_file) {
    std::string id = gen_uuid_v4();
    Cmd cmd;
    cmd.kind        = Cmd::Kind::BeginRun;
    cmd.run_id      = id;
    cmd.replay_file = replay_file;
    impl_->enqueue(std::move(cmd));
    return id;
}

void PostgresWriter::end_run(const std::string& run_id, uint64_t event_count) {
    Cmd cmd;
    cmd.kind        = Cmd::Kind::EndRun;
    cmd.run_id      = run_id;
    cmd.event_count = event_count;
    impl_->enqueue(std::move(cmd));
}

void PostgresWriter::write_trade(const Event& e, const std::string& run_id) {
    Cmd cmd;
    cmd.kind      = Cmd::Kind::Trade;
    cmd.run_id    = run_id;
    cmd.timestamp = e.timestamp;
    cmd.symbol_id = e.symbol_id;
    cmd.order_id  = e.order_id;
    cmd.price     = e.price;
    cmd.quantity  = e.quantity;
    cmd.side      = static_cast<uint8_t>(event_side(e));
    impl_->enqueue(std::move(cmd));
}

void PostgresWriter::flush() {
    auto done = std::make_shared<std::atomic_bool>(false);
    Cmd cmd;
    cmd.kind       = Cmd::Kind::Flush;
    cmd.flush_done = done;
    impl_->enqueue(std::move(cmd));
    while (!done->load(std::memory_order_acquire))
        std::this_thread::sleep_for(std::chrono::microseconds(200));
}

} // namespace trading
