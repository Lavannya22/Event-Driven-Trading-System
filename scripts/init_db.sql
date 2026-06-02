-- ── Phase 1/2: execution log ────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS executions (
    id          BIGSERIAL PRIMARY KEY,
    timestamp   BIGINT NOT NULL,
    symbol_id   INTEGER NOT NULL,
    order_id    BIGINT NOT NULL,
    price       BIGINT NOT NULL,
    quantity    BIGINT NOT NULL,
    side        SMALLINT NOT NULL,
    run_id      UUID NOT NULL
);

CREATE TABLE IF NOT EXISTS replay_runs (
    run_id      UUID PRIMARY KEY,
    started_at  TIMESTAMPTZ NOT NULL,
    ended_at    TIMESTAMPTZ,
    replay_file TEXT NOT NULL,
    event_count BIGINT
);

CREATE INDEX IF NOT EXISTS executions_run_id_idx    ON executions(run_id);
CREATE INDEX IF NOT EXISTS executions_symbol_id_idx ON executions(symbol_id);

-- ── Phase 4: backtesting + stability ────────────────────────────────────────

CREATE TABLE IF NOT EXISTS strategy_configs (
    config_id   UUID PRIMARY KEY,
    created_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
    config_json JSONB NOT NULL
);

CREATE TABLE IF NOT EXISTS backtest_runs (
    run_id      UUID PRIMARY KEY,
    started_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
    ended_at    TIMESTAMPTZ,
    replay_file TEXT NOT NULL,
    config_id   UUID REFERENCES strategy_configs(config_id),
    status      TEXT NOT NULL DEFAULT 'running'
);

CREATE TABLE IF NOT EXISTS backtest_results (
    result_id    UUID PRIMARY KEY,
    run_id       UUID NOT NULL REFERENCES backtest_runs(run_id),
    total_trades BIGINT,
    pnl          DOUBLE PRECISION,
    sharpe       DOUBLE PRECISION,
    win_rate     DOUBLE PRECISION,
    max_drawdown DOUBLE PRECISION
);

CREATE TABLE IF NOT EXISTS latency_results (
    result_id  UUID PRIMARY KEY,
    run_id     UUID NOT NULL REFERENCES backtest_runs(run_id),
    p50_ns     BIGINT,
    p99_ns     BIGINT,
    p999_ns    BIGINT,
    max_ns     BIGINT,
    throughput BIGINT
);

CREATE INDEX IF NOT EXISTS backtest_runs_config_id_idx    ON backtest_runs(config_id);
CREATE INDEX IF NOT EXISTS backtest_results_run_id_idx    ON backtest_results(run_id);
CREATE INDEX IF NOT EXISTS latency_results_run_id_idx     ON latency_results(run_id);
