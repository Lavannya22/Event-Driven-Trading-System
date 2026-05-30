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

CREATE INDEX IF NOT EXISTS executions_run_id_idx ON executions(run_id);
CREATE INDEX IF NOT EXISTS executions_symbol_id_idx ON executions(symbol_id);
