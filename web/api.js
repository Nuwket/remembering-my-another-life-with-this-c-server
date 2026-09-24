/**
 * api.js - Real HTTP transport + tracing.
 * Every lab button goes through this module. No mocks, no fake responses.
 */
const EXPLAIN_STATUS = {
  200: "OK — the server understood the request and answered.",
  201: "Created — a new row was stored in SQLite.",
  204: "No Content — deleted successfully; nothing to return.",
  400: "Bad Request — the server rejected the input.",
  401: "Unauthorized — this write needs an X-API-Key header.",
  403: "Forbidden — the key was sent but does not match.",
  404: "Not Found — this key, note or route does not exist.",
  405: "Wrong HTTP method — follow the Allow header (PUT for kv, POST for notes).",
  411: "PUT/POST needs a Content-Length header — fetch adds it automatically.",
  413: "Body exceeds the 64KB limit — shrink the payload.",
  415: "Set Content-Type: application/json.",
  500: "Server-side failure (disk/DB) — check the server logs.",
  501: "Chunked transfer-encoding is not implemented by this server."
};

const EXPLAIN_CODE = {
  missing_field: "A required JSON field is missing (value, title, body or data).",
  invalid_key: "Key must match [A-Za-z0-9._-], 1–128 chars.",
  invalid_id: "Note id must be a positive integer.",
  invalid_query: "Check limit (1–100) and offset (>= 0).",
  bad_request: "Malformed JSON, or title outside 1–200 chars.",
  unauthorized: "Send header X-API-Key: <token>. Get the token from whoever runs the server.",
  forbidden: "A key was sent but it is wrong. Check for typos.",
  not_found: "Nothing stored under that key/id.",
  method_not_allowed: "Use the method shown in the Allow header."
};

export function explain(status, bodyText) {
  let out = EXPLAIN_STATUS[status] || ("HTTP " + status + ".");
  try {
    const parsed = JSON.parse(bodyText);
    if (parsed && parsed.error && EXPLAIN_CODE[parsed.error]) out += " " + EXPLAIN_CODE[parsed.error];
  } catch (e) { /* non-JSON body: status text is enough */ }
  return out;
}

export function loadToken() {
  try { return localStorage.getItem("capi-key") || ""; } catch (e) { return ""; }
}

export function saveToken(value) {
  try { localStorage.setItem("capi-key", value); } catch (e) { /* private mode */ }
}

export function forgetToken() {
  try { localStorage.removeItem("capi-key"); } catch (e) { /* private mode */ }
}

/** Raw fetch. Returns {status, ms, bytes, text}. Never throws on HTTP status. */
export async function doFetch(method, path, raw, headers) {
  const options = { method, headers: Object.assign({}, headers) };
  if (raw && method !== "GET" && method !== "DELETE") options.body = raw;
  const started = performance.now();
  const response = await fetch(path, options);
  const text = await response.text();
  return { status: response.status, ms: Math.round(performance.now() - started), bytes: text.length, text };
}

/**
 * JSON-aware call with tracing, session counting and token attachment.
 * @param {object} ctx - {token, trace, onHttpError}
 */
export async function callJson(ctx, method, path, body, options) {
  const opts = options || {};
  const headers = Object.assign({}, opts.headers);
  let raw = "";
  if (body !== undefined) {
    raw = JSON.stringify(body);
    headers["Content-Type"] = "application/json";
  }
  if (!opts.noAuth && ctx.token && method !== "GET") headers["X-API-Key"] = ctx.token;
  const result = await doFetch(method, path, raw, headers);
  let pretty = result.text;
  try { pretty = JSON.stringify(JSON.parse(result.text), null, 2); } catch (e) { /* keep raw */ }
  if (opts.trace !== false) {
    ctx.trace({
      method, path, reqBody: raw, headers, status: result.status,
      ms: result.ms, bytes: result.bytes, respBody: result.text, label: opts.label
    });
    if (result.status < 200 || result.status >= 300) ctx.onHttpError(result.status, pretty);
  }
  return { status: result.status, body: pretty, ms: result.ms, bytes: result.bytes };
}

/** Build an equivalent curl command for a traced entry. */
export function buildCurl(method, path, headers, raw) {
  let command = "curl -X " + method + " " + location.origin + path;
  Object.keys(headers || {}).forEach((key) => {
    command += " -H '" + key + ": " + headers[key] + "'";
  });
  if (raw) command += " -d '" + raw.replace(/'/g, "'\\''") + "'";
  return command;
}

/** Parse a textarea of "Name: value" lines; throws on malformed lines. */
export function parseHeaders(text) {
  const out = {};
  const lines = text.split("\n");
  for (let i = 0; i < lines.length; i++) {
    const line = lines[i].trim();
    if (!line) continue;
    const colon = line.indexOf(":");
    if (colon <= 0) throw new Error("bad header line " + (i + 1) + ": " + line);
    out[line.slice(0, colon).trim()] = line.slice(colon + 1).trim();
  }
  return out;
}
