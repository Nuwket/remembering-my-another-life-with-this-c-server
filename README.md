# C Echo Server (thread-pool)

Minimal, correct, and observable TCP echo server in C11. Bounded thread-pool,
backpressure, graceful shutdown, strict warnings, ASan/UBSan clean, 15 tests.

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
                    echo loop
            recv (4KB stack) -> send_all
           partial/EINTR/timeout handled
```

Concurrency: acceptor + fixed workers. See `docs/ADR-001-concurrency-model.md`.
Observability: stderr structured logs `[LEVEL] time module=... peer=... msg=...`,
atomic stats (`connections_accepted/handled/dropped`, `echo_bytes`, `errors`).

## Build

Requirements: `gcc` (>=11), `make`, `pthread`. Optional: `clang-format`.

```bash
make
make test
make sanitize
make format-check
make clean
```

## Run

```bash
./build/server --help
./build/server --bind 127.0.0.1 --port 8080 --threads 8
# env overrides (CLI wins):
BIND_IP=0.0.0.0 PORT=8080 THREADS=8 ./build/server
```

Stop with `Ctrl-C` (SIGINT) or `kill -TERM <pid>`: listener closes, queue
drains, workers join, `stopped cleanly` is logged.

Quick echo check:

```bash
./build/server --port 8080 &
printf 'hello' | nc 127.0.0.1 8080 | xxd
kill %1
```

## Config / Env

| Flag | Env | Default | Notes |
|------|-----|---------|-------|
| `--bind IP` | `BIND_IP` | `0.0.0.0` | IPv4 only, validated with `inet_pton` |
| `--port PORT` | `PORT` | `8080` | 1-65535 |
| `--threads N` | `THREADS` | `8` | 1-64 workers |
| — | — | backlog 64, queue 128, 5s IO timeout | compile-time constants in `server.h` |

## Protocol / API

Wire: raw TCP echo. Client sends any bytes; server returns identical bytes
until EOF. No framing, no auth. Timeouts: 5s recv/send. Large payloads are
chunked through the 4KB stack buffer (verified with 64KB test).

Library API (`include/server.h`): `server_config_validate`,
`server_create`, `server_run` (blocking), `server_stop` (thread-safe,
idempotent), `server_destroy`, `server_stats`, `server_send_all`.
See header doc comments for ownership and thread-safety.

## Troubleshooting

- `bind/listen failed: Address already in use`: another process holds the
  port. Pick another `--port` or `lsof -i :8080`.
- `invalid --port/--threads`: check range (port 1-65535, threads 1-64).
- Client hangs: server has 5s IO timeout; check firewall / `nc -v`.
- `make sanitize` fails: read ASan trace (file:line), fix use-after-free /
  overflow, re-run `make test`.
- High `connections_dropped_queue_full`: queue saturated; increase
  `--threads` or queue size, or add client backoff.
