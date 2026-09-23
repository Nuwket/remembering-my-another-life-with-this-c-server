# ADR-001: Thread-Pool Concurrency Model

Status: Accepted
Date: 2026-09-23

## Context

This repository implements a minimal TCP echo server in C for correctness,
testability, and performance reasoning. We needed a concurrency model that is
simple to audit, bounded in memory/threads, provides backpressure, and passes
ASan/TSan cleanly.

## Decision

Use **acceptor + fixed worker thread-pool + bounded connection queue**:

- One acceptor loop (`server_run`) with `poll()` timeout (100ms) so
  `server_stop()` is honored promptly without signal-unsafe tricks.
- N workers (default 8, max 64) blocking on a mutex+condvar queue.
- Bounded queue (default 128): acceptor uses non-blocking `try_push`;
  when full, the connection is closed immediately and
  `connections_dropped_queue_full` is incremented.
- Per-connection handling uses a 4KB stack buffer, no malloc in hot path.
- Stats are C11 `atomic_ulong` counters (lock-free observability).

## Alternatives Considered

1. **Thread-per-connection**: simplest code, but unbounded thread/memory
   growth under many clients; thread creation cost per connection; harder
   to provide backpressure. Rejected for production safety.
2. **Single-threaded epoll**: highest throughput for 10k+ idle connections,
   fewer context switches. But significantly more complex state machine,
   harder to get partial send/recv + timeout + shutdown exactly right in a
   test-sized codebase. Rejected for YAGNI/KISS at this scale.
3. **Process-per-connection (fork)**: strong isolation, but high fork/exec
   cost and complex FD/metric sharing. Rejected.

## Consequences

- Positive: predictable memory (threads + queue + 4KB stack each),
  graceful shutdown (drain + join), easy to test (deterministic ports),
  no data races (short mutex scope, atomics for stats).
- Negative: throughput ceiling at N concurrent active connections; idle
  keep-alive connections occupy workers. Acceptable for echo/test workload.
- Migration path: if profiling shows acceptor saturation or 10k idle
  connections, replace acceptor+workers with epoll + small pool without
  changing `server.h` public API or wire protocol.
