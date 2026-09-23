# C HTTP API Server (thread-pool + SQLite)

Minimal, correct, and observable HTTP/1.1 JSON API in C11. Bounded
thread-pool, SQLite WAL storage, typed errors with correct status codes,
strict warnings, ASan/UBSan clean, 35 tests.

## Architecture

```
                +------------------+
                |   server_run     |  acceptor (poll 100ms)
                |  accept() loop   |
                +--------+---------+
                         | try_push (non-blocking)
                         v
                +------------------+
                | bounded queue    |  capacity 128, mutex+condvar
                | drop-if-full     |  -> close + metric
                +--------+---------+
                         | pop (blocking)
            +------------+------------+
            |            |            |
       worker 0 ... worker N (default 8)
            |            |            |
            +------------+------------+
                         |
              http_read_request (16KB headers + 64KB body)
                         |
                   api_dispatch
              router -> handlers -> store (SQLite, mutex)
```

Concurrency: acceptor + fixed workers (see `docs/ADR-001-concurrency-model.md`).
API + storage: HTTP/1.1 JSON + SQLite WAL (see `docs/ADR-002-http-sqlite.md`).
Observability: stderr structured logs, atomic stats, `/health`, `/metrics`.

Breaking change vs v0.1: raw TCP echo is now `POST /api/echo`.

## Build

Requirements: `gcc` (>=11), `make`, `pthread`, `libsqlite3-dev`. Optional: `clang-format`.

```bash
make
make test
make sanitize
make format-check
make clean
```

## Run

```bash
make run
# overrides:
make run RUN_PORT=8081 RUN_DB=./data/dev.db
# or directly:
./build/server --help
./build/server --bind 127.0.0.1 --port 8080 --threads 8 --db ./data/app.db
# env (CLI wins): BIND_IP, PORT, THREADS, DB_PATH
```

Stop with `Ctrl-C` (SIGINT) or `kill -TERM <pid>`: listener closes, queue
drains, workers join, WAL checkpoints, `stopped cleanly` is logged.

`make run` prints a banner with clickable shortcuts:

```
 C API Server v0.2.0 running
 App:      http://127.0.0.1:8080/
 Health:   http://127.0.0.1:8080/health
 Metrics:  http://127.0.0.1:8080/metrics
 KV:       http://127.0.0.1:8080/api/kv?limit=50
 Notes:    http://127.0.0.1:8080/api/notes?limit=50
```

## Console frontend (`GET /`)

The binary embeds a zero-dependency console (`web/index.html` via
`scripts/embed.py`, no CWD dependency): live health dot, metrics cards with
5s refresh, KV CRUD, Notes CRUD, echo probe, and shortcut links.

- **Dev Tools panel:** traces the last request (method, path, status, ms,
  request/response bodies), keeps a 15-entry history, and copies any request
  as a `curl` command.
- **Auto-explain (English):** every response is mapped to a plain-English
  cause plus fix (status codes and `error` codes like `missing_field`).
- **Languages:** UI strings in EN/PT/RU; default comes from
  `navigator.language` with a manual EN|PT|RU switch persisted in
  `localStorage`.

## Endpoints

| Method | Path | Body | Success | Errors |
|--------|------|------|---------|--------|
| GET | `/health` | — | 200 `{status,version,uptime_s}` | 405 |
| GET | `/metrics` | — | 200 `{connections_*,http_*,kv_count,notes_count}` | 405 |
| POST | `/api/echo` | `{"data":"hi"}` | 200 `{"data":"hi"}` | 400, 411, 415 |
| PUT | `/api/kv/:key` | `{"value":"..."}` | 200 `{key,value}` | 400, 411, 413, 415 |
| GET | `/api/kv/:key` | — | 200 `{key,value}` | 400, 404 |
| DELETE | `/api/kv/:key` | — | 204 | 400, 404 |
| GET | `/api/kv?prefix=&limit=` | — | 200 `{items:[...]}` | 400 |
| POST | `/api/notes` | `{"title","body"}` | 201 note | 400, 411, 413, 415 |
| GET | `/api/notes?limit=&offset=` | — | 200 `{items,total}` | 400 |
| GET | `/api/notes/:id` | — | 200 note | 400, 404 |
| PUT | `/api/notes/:id` | `{"title","body"}` | 200 note | 400, 404, 411, 415 |
| DELETE | `/api/notes/:id` | — | 204 | 400, 404 |

Key: `[A-Za-z0-9._-]{1,128}`. Title 1..200 chars, note body 0..8192.
`limit` 1..100 (default 50), `offset >= 0`. Errors are JSON:
`{"error":"not_found","message":"..."}` with `Allow` header on 405.

Quick check:

```bash
make run &
curl localhost:8080/health
curl -X PUT localhost:8080/api/kv/theme -H 'Content-Type: application/json' -d '{"value":"dark"}'
curl localhost:8080/api/kv/theme
curl -X POST localhost:8080/api/notes -H 'Content-Type: application/json' -d '{"title":"t1","body":"b1"}'
curl 'localhost:8080/api/notes?limit=10'
curl localhost:8080/metrics
kill %1
```

## Config / Env

| Flag | Env | Default | Notes |
|------|-----|---------|-------|
| `--bind IP` | `BIND_IP` | `0.0.0.0` | IPv4, validated with `inet_pton` |
| `--port PORT` | `PORT` | `8080` | 1-65535 |
| `--threads N` | `THREADS` | `8` | 1-64 workers |
| `--db PATH` | `DB_PATH` | `./data/app.db` | SQLite file, parent dirs created; failure aborts (no fallback) |

Compile-time: backlog 64, queue 128, 5s IO timeout, headers 16KB, body 64KB.

## Library API

`include/server.h`: `server_config_validate`, `server_create` (opens SQLite,
fails without fallback), `server_run`, `server_stop`, `server_destroy`
(checkpoints WAL), `server_stats` (+ `http_requests/http_errors`),
`server_uptime_s`, `server_send_all`.
`include/http.h`: typed `ApiError`, `http_parse_request`, `http_read_request`,
`http_respond`, `http_respond_error`, `http_query_get`.
`include/store.h`: `store_open/close`, `kv_put/get/del/list`,
`note_create/list/get/update/del`, `store_counts` (all thread-safe).
`include/api.h`: `api_dispatch`, `api_require_json`.

## Troubleshooting

- `bind/listen failed: Address already in use`: pick another `--port`.
- `store_open failed`: check `--db` path/permissions/disk space.
- `411 Length Required`: `PUT`/`POST` need `Content-Length`.
- `415`: send `-H 'Content-Type: application/json'`.
- `413`: body over 64KB; shrink payload.
- `405`: check `Allow` header for the right method.
- `make sanitize` fails: read ASan trace, fix, re-run `make test`.
- High `connections_dropped_queue_full`: increase `--threads` or add backoff.
