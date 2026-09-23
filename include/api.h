#ifndef API_H
#define API_H

#include "http.h"
#include "server.h"
#include "store.h"

/*
 * api_dispatch - Route one parsed request to its handler and respond.
 * @fd: client socket. @req: parsed request (body aliases caller buffer).
 * @store: SQLite store (thread-safe). @server: for stats/uptime (thread-safe).
 * Returns: API_OK when a response was sent, else the typed error that was
 *   already rendered as JSON with the correct status. Never falls back to 200.
 * Thread-safe (per-connection state only, store/server are internally locked).
 */
ApiError api_dispatch(int fd, const HttpRequest *req, Store *store, Server *server);

/*
 * api_require_json - Enforce Content-Type: application/json for bodies.
 * Returns: API_OK or API_ERR_UNSUPPORTED_MEDIA / LENGTH_REQUIRED explicitly.
 */
ApiError api_require_json(const HttpRequest *req, int body_required);

#endif
