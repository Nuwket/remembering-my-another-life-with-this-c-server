<div align="center">

<img src="docs/assets/reactor-core.svg" alt="Nuclear reactor core mark" width="620">

# Nuclear C API Server

**A hand-written HTTP/1.1 JSON API server in C, presented as a reactor control panel.**

`thread-pool` &middot; `SQLite WAL` &middot; `typed errors` &middot; `zero runtime dependencies`

---

```bash
make run
```

</div>

> The "radiation" readouts are **visual theme only** — they are not measured
> values. Every number the server reports comes from a real counter.

---

## Features

- **HTTP/1.1** server written from scratch: request line, headers, `Content-Length`, bounded body
- **JSON** encode/decode with strict validation and typed error codes
- **SQLite (WAL)** storage with prepared statements, `busy_timeout`, checkpoint on shutdown
- **Bounded thread pool** with a fixed connection queue and explicit backpressure
- **Optional API key** (`X-API-Key`) with constant-time comparison: `401` missing, `403` wrong
- **Typed errors** mapped 1:1 to status codes — no silent fallbacks anywhere
- **Observability** — structured logs, `/health`, `/metrics`, per-request inspector in the lab
- **Embedded lab** — a single-file interactive playground served at `GET /`
- **Strict build** — `-Wall -Wextra -Werror -Wconversion -Wshadow -pedantic`, ASan/UBSan clean
- **Terminal UI** — responsive box layout, boot sequence, UTF-8 with ASCII fallback

## Quick Start

```bash
# dependencies
sudo apt install build-essential libsqlite3-dev   # Debian/Ubuntu

make run                                            # boot + start on :8080
make run RUN_API_KEY=demo                           # protected mode (401/403)
make run RUN_PORT=8081 RUN_DB=./data/dev.db         # custom port and database
```

Then open **<http://127.0.0.1:8080/>** — every lab section ships prefilled
examples, so you can click through save, fetch, delete, notes, auth, errors
and a live performance run without writing a request by hand.

```bash
curl localhost:8080/health
curl -X PUT localhost:8080/api/kv/theme \
     -H 'Content-Type: application/json' -d '{"value":"dark"}'
curl localhost:8080/api/kv/theme
```

## Commands

| Command | Purpose |
|---------|---------|
| `make` | build |
| `make run` | clear screen, boot animation, start the server |
| `make test` | unit + integration tests |
| `make sanitize` | same suite under ASan + UBSan |
| `make format-check` | `clang-format` in check mode |
| `make clean` | remove build artifacts |

## Configuration

| Flag | Env | Default | Notes |
|------|-----|---------|-------|
| `--bind IP` | `BIND_IP` | `0.0.0.0` | IPv4, validated at the boundary |
| `--port PORT` | `PORT` | `8080` | 1-65535 |
| `--threads N` | `THREADS` | `8` | 1-64 workers |
| `--db PATH` | `DB_PATH` | `./data/app.db` | parent directories are created |
| `--api-key KEY` | `API_KEY` | *(unset)* | enables protected mode for writes |

`NO_COLOR`, `COLUMNS`, `CI` and `CONSOLE_NO_ANIMATION` are respected by the
terminal UI. `LANG`/`LC_ALL` decide between Unicode box drawing and the plain
ASCII fallback.

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

## Run

```bash
make run
# overrides:
make run RUN_PORT=8081 RUN_DB=./data/dev.db RUN_API_KEY=secret
# or directly:
./build/server --help
./build/server --bind 127.0.0.1 --port 8080 --threads 8 --db ./data/app.db --api-key secret
# env (CLI wins): BIND_IP, PORT, THREADS, DB_PATH, API_KEY
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

## Lab frontend (`GET /`)

The binary embeds a zero-dependency interactive lab (`web/index.html` via
`scripts/embed.py`, no CWD dependency). No mocks: every control calls the
live C backend. Rule of the page: function first, tech second — anyone can
use it without knowing what an API is.

- **Save/fetch/delete flow** with human verdicts ("the server saved it"),
  visual note cards with edit/delete, vault narrative that adapts to
  open/protected mode, echo as an experience, and a 6-card error gallery.
- **Performance runner:** 1/10/100 real requests with total/average timing,
  measured in the browser, nothing faked.
- **How-it-works** expandables per feature plus a collapsed **tech mode**
  (raw HTTP composer, last-call detail, history, curl copy, session matrix).
- **Language comparison:** per-function tabs (save/get/del/note/echo/auth)
  across C/Python/Go/Rust/Node with side-by-side view, copy buttons, LOC and
  explicit library labels. C tabs show real shortened snippets from this
  server's `src/`; every other tab is labeled "equivalent example — NOT
  executed here". A languages section adds descriptive per-language notes and
  an honest piece-by-piece matrix (HTTP/JSON/SQLite/errors/concurrency/
  memory). UI in EN/PT/RU via `navigator.language`.

## Auth

Optional API key. Without `--api-key`/`API_KEY` the server is **open**.
With it set, writes (`PUT`/`POST`/`DELETE` under `/api/`) require header
`X-API-Key`: missing/empty → `401 unauthorized`, mismatch → `403 forbidden`
(constant-time compare). Reads stay public. `/health` reports
`"auth":"open"` or `"auth":"protected"`. Keys compare silently — the value
is never logged.

## Endpoints

| Method | Path | Body | Success | Errors |
|--------|------|------|---------|--------|
| GET | `/health` | — | 200 `{status,version,uptime_s,auth}` | 405 |
| GET | `/metrics` | — | 200 `{connections_*,http_*,kv_count,notes_count}` | 405 |
| POST | `/api/echo` | `{"data":"hi"}` | 200 `{"data":"hi"}` | 400, 411, 415 |
| PUT | `/api/kv/:key` | `{"value":"..."}` | 200 `{key,value}` | 400, 401, 403, 411, 413, 415 |
| GET | `/api/kv/:key` | — | 200 `{key,value}` | 400, 404 |
| DELETE | `/api/kv/:key` | — | 204 | 400, 401, 403, 404 |
| GET | `/api/kv?prefix=&limit=` | — | 200 `{items:[...]}` | 400 |
| POST | `/api/notes` | `{"title","body"}` | 201 note | 400, 401, 403, 411, 413, 415 |
| GET | `/api/notes?limit=&offset=` | — | 200 `{items,total}` | 400 |
| GET | `/api/notes/:id` | — | 200 note | 400, 404 |
| PUT | `/api/notes/:id` | `{"title","body"}` | 200 note | 400, 401, 403, 404, 411, 415 |
| DELETE | `/api/notes/:id` | — | 204 | 400, 401, 403, 404 |

401/403 only in protected mode (`--api-key` set); otherwise writes are open.

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
