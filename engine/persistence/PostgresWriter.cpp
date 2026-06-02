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
    const uint32_t c  = (d(gen) & 0x0FFFu) | 0x4000u;
    const uint32_t dv = (d(gen) & 0x3FFFu) | 0x8000u;
    const uint32_t e1 = d(gen);
    const uint16_t e2 = static_cast<uint16_t>(d(gen));
    char buf[37];
    std::snprintf(buf, sizeof(buf), "%08x-%04x-%04x-%04x-%08x%04x",
                  a, b & 0xFFFFu, c, dv, e1, static_cast<unsigned>(e2));
    return buf;
}

// ── Internal command ──────────────────────────────────────────────────────────
struct Cmd {
    enum class Kind {
        Trade, BeginRun, EndRun,
        WriteStrategyConfig, BeginBacktestRun, EndBacktestRun,
        WriteBacktestResult, WriteLatencyResult,
        Flush, Shutdown
    };
    Kind        kind{Kind::Shutdown};
    std::string run_id;
    std::string config_id;
    std::string replay_file;
    std::string status;
    std::string config_json;

    // Trade fields
    uint64_t timestamp{0};
    uint32_t symbol_id{0};
    uint64_t order_id{0};
    uint64_t price{0};
    uint64_t quantity{0};
    uint8_t  side{0};

    // EndRun
    uint64_t event_count{0};

    // BacktestResult
    uint64_t total_trades{0};
    double   pnl{0};
    double   sharpe{0};
    double   win_rate{0};
    double   max_drawdown{0};

    // LatencyResult
    uint64_t p50_ns{0};
    uint64_t p99_ns{0};
    uint64_t p999_ns{0};
    uint64_t max_ns{0};
    uint64_t throughput_eps{0};

    std::atomic<bool>* flush_done{nullptr};
};

// ── Impl ──────────────────────────────────────────────────────────────────────
struct PostgresWriter::Impl {
    Config cfg_;
    bool   connected_{false};
    std::atomic<uint64_t> overflow_count_{0};

#ifdef WITH_POSTGRES
    PGconn* conn_{nullptr};
#endif

    std::mutex              mtx_;
    std::condition_variable cv_;
    std::queue<Cmd>         queue_;
    std::thread             thread_;

    // Pause mechanism (test-only)
    bool paused_{false};
    std::condition_variable pause_cv_;

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
            // Flush and Shutdown bypass the depth limit — dropping them would
            // deadlock flush() or the destructor respectively.
            const bool is_control = (cmd.kind == Cmd::Kind::Flush ||
                                     cmd.kind == Cmd::Kind::Shutdown);
            if (!is_control && queue_.size() >= PostgresWriter::MAX_QUEUE_DEPTH) {
                overflow_count_.fetch_add(1, std::memory_order_relaxed);
                return;
            }
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
                // Honour pause before processing (test helper)
                pause_cv_.wait(lk, [this] { return !paused_; });
                cmd = std::move(queue_.front());
                queue_.pop();
            }
            switch (cmd.kind) {
                case Cmd::Kind::Shutdown:            return;
                case Cmd::Kind::Flush:               exec_flush(cmd);                  break;
                case Cmd::Kind::BeginRun:            exec_begin_run(cmd);              break;
                case Cmd::Kind::EndRun:              exec_end_run(cmd);                break;
                case Cmd::Kind::Trade:               exec_trade(cmd);                  break;
                case Cmd::Kind::WriteStrategyConfig: exec_write_strategy_config(cmd);  break;
                case Cmd::Kind::BeginBacktestRun:    exec_begin_backtest_run(cmd);     break;
                case Cmd::Kind::EndBacktestRun:      exec_end_backtest_run(cmd);       break;
                case Cmd::Kind::WriteBacktestResult: exec_write_backtest_result(cmd);  break;
                case Cmd::Kind::WriteLatencyResult:  exec_write_latency_result(cmd);   break;
            }
        }
    }

    // ── Phase 1/2 exec ────────────────────────────────────────────────────

    void exec_flush(const Cmd& cmd) {
        if (cmd.flush_done)
            cmd.flush_done->store(true, std::memory_order_release);
    }

    void exec_begin_run(const Cmd& cmd) {
#ifdef WITH_POSTGRES
        if (!conn_) return;
        const char* params[2] = {cmd.run_id.c_str(), cmd.replay_file.c_str()};
        PGresult* res = PQexecParams(conn_,
            "INSERT INTO replay_runs(run_id,started_at,replay_file)"
            " VALUES($1::uuid,now(),$2)",
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
            "UPDATE replay_runs SET ended_at=now(),event_count=$1"
            " WHERE run_id=$2::uuid",
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
            "(timestamp,symbol_id,order_id,price,quantity,side,run_id)"
            " VALUES($1,$2,$3,$4,$5,$6,$7::uuid)",
            7, nullptr, params, nullptr, nullptr, 0);
        if (PQresultStatus(res) != PGRES_COMMAND_OK)
            std::cerr << "[persistence] write_trade: " << PQerrorMessage(conn_);
        PQclear(res);
#else
        (void)cmd;
#endif
    }

    // ── Phase 4 exec ─────────────────────────────────────────────────────

    void exec_write_strategy_config(const Cmd& cmd) {
#ifdef WITH_POSTGRES
        if (!conn_) return;
        const char* params[2] = {cmd.config_id.c_str(), cmd.config_json.c_str()};
        PGresult* res = PQexecParams(conn_,
            "INSERT INTO strategy_configs(config_id,created_at,config_json)"
            " VALUES($1::uuid,now(),$2::jsonb)"
            " ON CONFLICT(config_id) DO NOTHING",
            2, nullptr, params, nullptr, nullptr, 0);
        if (PQresultStatus(res) != PGRES_COMMAND_OK)
            std::cerr << "[persistence] write_strategy_config: " << PQerrorMessage(conn_);
        PQclear(res);
#else
        (void)cmd;
#endif
    }

    void exec_begin_backtest_run(const Cmd& cmd) {
#ifdef WITH_POSTGRES
        if (!conn_) return;
        const char* cid = cmd.config_id.empty() ? nullptr : cmd.config_id.c_str();
        const char* params[3] = {cmd.run_id.c_str(), cmd.replay_file.c_str(), cid};
        PGresult* res = PQexecParams(conn_,
            "INSERT INTO backtest_runs(run_id,started_at,replay_file,config_id,status)"
            " VALUES($1::uuid,now(),$2,$3::uuid,'running')",
            3, nullptr, params, nullptr, nullptr, 0);
        if (PQresultStatus(res) != PGRES_COMMAND_OK)
            std::cerr << "[persistence] begin_backtest_run: " << PQerrorMessage(conn_);
        PQclear(res);
#else
        (void)cmd;
#endif
    }

    void exec_end_backtest_run(const Cmd& cmd) {
#ifdef WITH_POSTGRES
        if (!conn_) return;
        const char* params[2] = {cmd.status.c_str(), cmd.run_id.c_str()};
        PGresult* res = PQexecParams(conn_,
            "UPDATE backtest_runs SET ended_at=now(),status=$1"
            " WHERE run_id=$2::uuid",
            2, nullptr, params, nullptr, nullptr, 0);
        if (PQresultStatus(res) != PGRES_COMMAND_OK)
            std::cerr << "[persistence] end_backtest_run: " << PQerrorMessage(conn_);
        PQclear(res);
#else
        (void)cmd;
#endif
    }

    void exec_write_backtest_result(const Cmd& cmd) {
#ifdef WITH_POSTGRES
        if (!conn_) return;
        std::string result_id = gen_uuid_v4();
        char trades[24], pnl[32], sharpe[32], wr[32], dd[32];
        std::snprintf(trades, sizeof(trades), "%" PRIu64, cmd.total_trades);
        std::snprintf(pnl,    sizeof(pnl),    "%.6f",    cmd.pnl);
        std::snprintf(sharpe, sizeof(sharpe), "%.6f",    cmd.sharpe);
        std::snprintf(wr,     sizeof(wr),     "%.6f",    cmd.win_rate);
        std::snprintf(dd,     sizeof(dd),     "%.6f",    cmd.max_drawdown);
        const char* params[7] = {result_id.c_str(), cmd.run_id.c_str(),
                                  trades, pnl, sharpe, wr, dd};
        PGresult* res = PQexecParams(conn_,
            "INSERT INTO backtest_results"
            "(result_id,run_id,total_trades,pnl,sharpe,win_rate,max_drawdown)"
            " VALUES($1::uuid,$2::uuid,$3,$4,$5,$6,$7)",
            7, nullptr, params, nullptr, nullptr, 0);
        if (PQresultStatus(res) != PGRES_COMMAND_OK)
            std::cerr << "[persistence] write_backtest_result: " << PQerrorMessage(conn_);
        PQclear(res);
#else
        (void)cmd;
#endif
    }

    void exec_write_latency_result(const Cmd& cmd) {
#ifdef WITH_POSTGRES
        if (!conn_) return;
        std::string result_id = gen_uuid_v4();
        char p50[24], p99[24], p999[24], mx[24], tp[24];
        std::snprintf(p50,  sizeof(p50),  "%" PRIu64, cmd.p50_ns);
        std::snprintf(p99,  sizeof(p99),  "%" PRIu64, cmd.p99_ns);
        std::snprintf(p999, sizeof(p999), "%" PRIu64, cmd.p999_ns);
        std::snprintf(mx,   sizeof(mx),   "%" PRIu64, cmd.max_ns);
        std::snprintf(tp,   sizeof(tp),   "%" PRIu64, cmd.throughput_eps);
        const char* params[7] = {result_id.c_str(), cmd.run_id.c_str(),
                                  p50, p99, p999, mx, tp};
        PGresult* res = PQexecParams(conn_,
            "INSERT INTO latency_results"
            "(result_id,run_id,p50_ns,p99_ns,p999_ns,max_ns,throughput)"
            " VALUES($1::uuid,$2::uuid,$3,$4,$5,$6,$7)",
            7, nullptr, params, nullptr, nullptr, 0);
        if (PQresultStatus(res) != PGRES_COMMAND_OK)
            std::cerr << "[persistence] write_latency_result: " << PQerrorMessage(conn_);
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

uint64_t PostgresWriter::overflow_count() const noexcept {
    return impl_->overflow_count_.load(std::memory_order_relaxed);
}

// ── Phase 1/2 ────────────────────────────────────────────────────────────────

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

// ── Phase 4 ───────────────────────────────────────────────────────────────────

std::string PostgresWriter::write_strategy_config(const StrategyConfig& cfg) {
    std::string id = gen_uuid_v4();
    char json[256];
    std::snprintf(json, sizeof(json),
        "{\"window_size\":%u,\"threshold\":%.6f,\"inventory_limit\":%.6f}",
        cfg.window_size, cfg.threshold, cfg.inventory_limit);
    Cmd cmd;
    cmd.kind        = Cmd::Kind::WriteStrategyConfig;
    cmd.config_id   = id;
    cmd.config_json = json;
    impl_->enqueue(std::move(cmd));
    return id;
}

std::string PostgresWriter::begin_backtest_run(const std::string& replay_file,
                                                const std::string& config_id) {
    std::string id = gen_uuid_v4();
    Cmd cmd;
    cmd.kind        = Cmd::Kind::BeginBacktestRun;
    cmd.run_id      = id;
    cmd.replay_file = replay_file;
    cmd.config_id   = config_id;
    impl_->enqueue(std::move(cmd));
    return id;
}

void PostgresWriter::end_backtest_run(const std::string& run_id,
                                       const std::string& status) {
    Cmd cmd;
    cmd.kind   = Cmd::Kind::EndBacktestRun;
    cmd.run_id = run_id;
    cmd.status = status;
    impl_->enqueue(std::move(cmd));
}

void PostgresWriter::write_backtest_result(const std::string& run_id,
                                            const RunResult& result) {
    Cmd cmd;
    cmd.kind         = Cmd::Kind::WriteBacktestResult;
    cmd.run_id       = run_id;
    cmd.total_trades = result.total_trades;
    cmd.pnl          = result.realized_pnl;
    cmd.sharpe       = result.sharpe_ratio;
    cmd.win_rate     = result.win_rate;
    cmd.max_drawdown = result.max_drawdown;
    impl_->enqueue(std::move(cmd));
}

void PostgresWriter::write_latency_result(const std::string& run_id,
                                           uint64_t p50_ns, uint64_t p99_ns,
                                           uint64_t p999_ns, uint64_t max_ns,
                                           uint64_t throughput_eps) {
    Cmd cmd;
    cmd.kind          = Cmd::Kind::WriteLatencyResult;
    cmd.run_id        = run_id;
    cmd.p50_ns        = p50_ns;
    cmd.p99_ns        = p99_ns;
    cmd.p999_ns       = p999_ns;
    cmd.max_ns        = max_ns;
    cmd.throughput_eps= throughput_eps;
    impl_->enqueue(std::move(cmd));
}

void PostgresWriter::pause_writer() noexcept {
    std::lock_guard lk(impl_->mtx_);
    impl_->paused_ = true;
}

void PostgresWriter::resume_writer() noexcept {
    {
        std::lock_guard lk(impl_->mtx_);
        impl_->paused_ = false;
    }
    impl_->pause_cv_.notify_all();
}

void PostgresWriter::flush() {
    std::atomic<bool> done{false};
    Cmd cmd;
    cmd.kind       = Cmd::Kind::Flush;
    cmd.flush_done = &done;
    impl_->enqueue(std::move(cmd));
    while (!done.load(std::memory_order_acquire))
        std::this_thread::sleep_for(std::chrono::microseconds(200));
}

} // namespace trading
