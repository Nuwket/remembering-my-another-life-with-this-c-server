# ADR-002: HTTP/1.1 JSON API with SQLite Storage

Status: Accepted
Date: 2026-09-23

## Context

The server started as a raw TCP echo service. To demonstrate request handling,
validation, routing, and local persistence in C, we needed application-level
endpoints testable with `curl` and backed by durable storage.

## Decision

- **Protocol: minimal HTTP/1.1 + JSON, no external deps.** One request per
  connection (`Connection: close`). Parser rejects malformed input with typed
  `ApiError` values; `Transfer-Encoding: chunked` returns `501` explicitly.
- **Storage: SQLite (libsqlite3) with WAL.** Single connection guarded by a
  mutex (thread-safe, documented). `busy_timeout=5000`, `synchronous=NORMAL`,
  prepared statements per call, `wal_checkpoint(TRUNCATE)` on shutdown.
  Tables: `kv(key PK, value, updated_at)`, `notes(id PK, title, body,
  created_at, updated_at)`.
- **Errors: typed, no silent fallback.** `ApiError` maps 1:1 to status codes:
  `400` bad/missing/invalid, `404` not found, `405` + `Allow`, `411` missing
  length, `413` over limit, `415` wrong content-type, `501` chunked,
  `500` internal. Unknown enum values map to `500`, never `200`.
- **Limits:** headers 16KB, body 64KB, key `[A-Za-z0-9._-]{1,128}`, value
  60KB, title 1..200, note body 8KB, list `limit` 1..100.

## Alternatives Considered

1. **Custom text protocol:** simpler parser, but not `curl`-testable and less
   portfolio value. Rejected.
2. **Append-only JSONL file:** zero deps and easy to reason about, but needs
   hand-rolled locking, compaction, and crash recovery. SQLite gives WAL,
   atomic commits, and indexes for free. Rejected (kept as documented
   migration-down option for dependency-free builds).
3. **Per-thread SQLite connections:** higher write concurrency, but complex
   lifecycle and WAL contention tuning. Rejected for KISS; single mutex is
   correct at 8 workers and easy to profile before optimizing.

## Consequences

- Positive: durable CRUD (`/api/kv`, `/api/notes`), observable
  (`/health`, `/metrics`), fully tested status codes, ASan/UBSan clean.
- Negative: breaking change — raw echo is now `POST /api/echo`; workers can
  block on SQLite writes under contention; `Connection: close` only (no
  keep-alive yet).
- Migration path: keep-alive, per-thread connections, or JSONL backend can
  replace internals without changing routes or status codes.
