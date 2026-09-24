/**
 * lab.js — behaviours for the Nuclear API Lab.
 *
 * The C server does all the work. This file only translates intent into real
 * requests and real responses into human verdicts. No mocked data anywhere.
 */
import { I18N, MSGS, TIPS, HOW, detectLang } from "./i18n.js";
import { CMP, IMPL, IMPLNAME, LANGNOTES, PIECES, PIECE_L } from "./compare.js";
import { callJson, doFetch, buildCurl, parseHeaders, explain, loadToken, saveToken, forgetToken } from "./api.js";

const TOKEN_KEY = "capi-key";
const LANG_KEY = "capi-lang";
const HISTORY_LIMIT = 15;

let lang = detectLang();
let token = loadToken();
let protectedMode = false;
let lastCall = null;
let currentImpl = "c";
let perfCount = 10;

const history = [];
const session = new Map();
const els = new Map();

const $ = (id) => document.getElementById(id);
const cache = (id) => {
  if (!els.has(id)) els.set(id, $(id));
  return els.get(id);
};

/* ------------------------------------------------------------------ *
 * Toasts
 * ------------------------------------------------------------------ */

function toast(message, tone) {
  const box = cache("toasts");
  const item = document.createElement("div");
  item.className = "toast";
  if (tone) item.dataset.tone = tone;
  item.textContent = message;
  box.appendChild(item);
  setTimeout(() => item.remove(), 5200);
}

/* ------------------------------------------------------------------ *
 * i18n
 * ------------------------------------------------------------------ */

function applyLanguage(code) {
  if (!I18N[code]) code = "en";
  lang = code;
  document.documentElement.lang = code;
  const dict = I18N[code];

  document.querySelectorAll("[data-i18n]").forEach((el) => {
    const key = el.getAttribute("data-i18n");
    if (dict[key] !== undefined) el.textContent = dict[key];
  });
  document.querySelectorAll("[data-i18n-ph]").forEach((el) => {
    const key = el.getAttribute("data-i18n-ph");
    if (dict[key] !== undefined) el.placeholder = dict[key];
  });
  document.querySelectorAll("[data-tip]").forEach((el) => {
    const key = el.getAttribute("data-tip");
    if (TIPS[code] && TIPS[code][key]) el.title = TIPS[code][key];
  });
  document.querySelectorAll("[data-how]").forEach((el) => {
    const key = el.getAttribute("data-how");
    if (HOW[code] && HOW[code][key]) el.innerHTML = HOW[code][key];
  });
  document.querySelectorAll("#langSwitch button").forEach((btn) => {
    btn.setAttribute("aria-pressed", btn.getAttribute("data-lang") === code ? "true" : "false");
  });

  try { localStorage.setItem(LANG_KEY, code); } catch (e) { /* private mode */ }

  document.querySelectorAll("[data-fn]").forEach((host) => {
    if (host._selected) renderCompare(host);
  });
  renderMatrix();
  renderAuthBadge();
}

/* ------------------------------------------------------------------ *
 * Readouts
 * ------------------------------------------------------------------ */

function paint(id, text, ok, meta) {
  const el = cache(id);
  el.textContent = text;
  el.dataset.state = ok === undefined ? "idle" : ok ? "ok" : "fail";
  if (meta !== undefined) {
    const tail = document.createElement("span");
    tail.className = "meta";
    tail.textContent = meta;
    el.appendChild(tail);
  }
}

function line(method, path, result, extra) {
  let text = method + " " + path + "  →  HTTP " + result.status;
  if (result.ms !== undefined) text += "  ·  " + result.ms + "ms  ·  " + result.bytes + " bytes";
  if (extra) text += "  ·  " + extra;
  return text;
}

function idle(id) {
  const el = cache(id);
  el.textContent = I18N[lang].idle;
  el.dataset.state = "idle";
}

function begin(id, message) {
  const el = cache(id);
  el.textContent = message;
  el.dataset.state = "busy";
}

/* ------------------------------------------------------------------ *
 * Tracing
 * ------------------------------------------------------------------ */

function trace(entry) {
  lastCall = entry;
  history.unshift(entry);
  if (history.length > HISTORY_LIMIT) history.pop();

  cache("lastCall").textContent =
    entry.method + " " + entry.path + "\n" +
    "→ HTTP " + entry.status + "  ·  " + entry.ms + "ms  ·  " + entry.bytes + " bytes\n\n" +
    "request:\n" + (entry.reqBody || "(empty)") + "\n\nresponse:\n" + entry.respBody;

  cache("history").textContent = history
    .map((item, index) => {
      const when = new Date().toLocaleTimeString();
      return (index === 0 ? "▸ " : "  ") + when + "  " + item.method + " " + item.path +
        "  →  " + item.status + "  (" + item.ms + "ms)";
    })
    .join("\n");

  cache("explainText").textContent = explain(entry.status, entry.respBody);
  countSession(entry.label || entry.method + " " + entry.path, entry.status);
}

function countSession(label, status) {
  if (!label) return;
  const bucket = status >= 200 && status < 300 ? "ok" : status >= 400 && status < 500 ? "c4" : "c5";
  const row = session.get(label) || { ok: 0, c4: 0, c5: 0 };
  row[bucket] += 1;
  session.set(label, row);
  renderSession();
}

function renderSession() {
  const body = cache("sessionBody");
  if (!session.size) {
    body.innerHTML = '<tr><td colspan="5">—</td></tr>';
    return;
  }
  const rows = [...session.keys()].sort().map((label) => {
    const counts = session.get(label);
    const total = counts.ok + counts.c4 + counts.c5;
    return "<tr><td>" + escapeHtml(label) + "</td><td>" + counts.ok + "</td><td>" + counts.c4 +
      "</td><td>" + counts.c5 + "</td><td>" + total + "</td></tr>";
  });
  body.innerHTML = rows.join("");
}

function escapeHtml(value) {
  return String(value).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
}

/* ------------------------------------------------------------------ *
 * Busy button helper
 * ------------------------------------------------------------------ */

async function busy(button, action) {
  if (button.dataset.busy === "1") return;
  button.dataset.busy = "1";
  button.setAttribute("aria-busy", "true");
  try {
    await action();
  } catch (error) {
    toast(I18N[lang].networkError + ": " + error, "fail");
  } finally {
    delete button.dataset.busy;
    button.removeAttribute("aria-busy");
  }
}

/* ------------------------------------------------------------------ *
 * KV
 * ------------------------------------------------------------------ */

function kvFields() {
  return { key: cache("kvKey").value.trim(), value: cache("kvValue").value };
}

async function kvSave() {
  const { key, value } = kvFields();
  const path = "/api/kv/" + encodeURIComponent(key);
  begin("kvOut", I18N[lang].busyWrite);
  const result = await callJson(ctx, "PUT", path, { value }, { label: "PUT /api/kv/:key" });
  const ok = result.status >= 200 && result.status < 300;
  paint("kvOut", ok ? MSGS[lang].saved(key, value) : MSGS[lang].badRequest, ok, line("PUT", path, result));
  refreshStats(true);
}

async function kvFetch() {
  const { key } = kvFields();
  const path = "/api/kv/" + encodeURIComponent(key);
  begin("kvOut", I18N[lang].busyReq);
  const result = await callJson(ctx, "GET", path, undefined, { label: "GET /api/kv/:key" });
  let human = MSGS[lang].notFound();
  if (result.status === 200) {
    try { human = MSGS[lang].fetched(JSON.parse(result.body).value); }
    catch (e) { human = MSGS[lang].fetched(result.body); }
  }
  paint("kvOut", human, result.status === 200, line("GET", path, result));
}

async function kvDelete() {
  const { key } = kvFields();
  if (!window.confirm(I18N[lang].confirmKv + " " + key + "?")) return;
  const path = "/api/kv/" + encodeURIComponent(key);
  begin("kvOut", I18N[lang].busyWrite);
  const result = await callJson(ctx, "DELETE", path, undefined, { label: "DELETE /api/kv/:key" });
  const ok = result.status === 204;
  paint("kvOut", ok ? MSGS[lang].deleted() : MSGS[lang].notFound(), ok, line("DELETE", path, result));
  refreshStats(true);
}

/* ------------------------------------------------------------------ *
 * Notes
 * ------------------------------------------------------------------ */

function noteFields() {
  return {
    title: cache("noteTitleValue").value,
    body: cache("noteBodyValue").value
  };
}

function renderNotes(items) {
  const host = cache("noteList");
  host.textContent = "";
  items.forEach((note) => {
    const row = document.createElement("div");
    row.className = "item";

    const body = document.createElement("div");
    body.className = "item-body";

    const title = document.createElement("div");
    title.className = "item-title";
    const id = document.createElement("span");
    id.className = "item-id";
    id.textContent = "#" + note.id;
    const text = document.createElement("span");
    text.textContent = note.title;
    title.append(id, text);

    const content = document.createElement("p");
    content.className = "item-text";
    content.textContent = note.body;

    body.append(title, content);

    const tools = document.createElement("div");
    tools.className = "item-actions";

    const edit = document.createElement("button");
    edit.className = "icon-btn";
    edit.type = "button";
    edit.textContent = "✎";
    edit.title = I18N[lang].editNote;
    edit.setAttribute("aria-label", I18N[lang].editNote + " " + note.id);
    edit.addEventListener("click", () => {
      cache("noteTitleValue").value = note.title;
      cache("noteBodyValue").value = note.body;
      noteUpdate(note.id);
    });

    const remove = document.createElement("button");
    remove.className = "icon-btn is-danger";
    remove.type = "button";
    remove.textContent = "✕";
    remove.title = I18N[lang].deleteNote;
    remove.setAttribute("aria-label", I18N[lang].deleteNote + " " + note.id);
    remove.addEventListener("click", () => noteDelete(note.id));

    tools.append(edit, remove);
    row.append(body, tools);
    host.appendChild(row);
  });
}

async function noteCreate() {
  const { title, body } = noteFields();
  begin("noteOut", I18N[lang].busyWrite);
  const result = await callJson(ctx, "POST", "/api/notes", { title, body }, { label: "POST /api/notes" });
  let human = MSGS[lang].badRequest();
  if (result.status === 201) {
    try { human = MSGS[lang].noteCreated(JSON.parse(result.body).id); }
    catch (e) { human = MSGS[lang].noteCreated("?"); }
  }
  paint("noteOut", human, result.status === 201, line("POST", "/api/notes", result));
  refreshStats(true);
  noteList();
}

async function noteList() {
  const path = "/api/notes?limit=50";
  begin("noteOut", I18N[lang].busyReq);
  const result = await callJson(ctx, "GET", path, undefined, { label: "GET /api/notes" });
  if (result.status !== 200) {
    paint("noteOut", MSGS[lang].badRequest(), false, line("GET", path, result));
    renderNotes([]);
    return;
  }
  let items = [];
  let total = 0;
  try {
    const parsed = JSON.parse(result.body);
    items = parsed.items || [];
    total = parsed.total !== undefined ? parsed.total : items.length;
  } catch (e) { /* fall through to empty */ }
  paint("noteOut", MSGS[lang].notesListed(total), true, line("GET", path, result));
  renderNotes(items);
}

async function noteUpdate(id) {
  const { title, body } = noteFields();
  const path = "/api/notes/" + id;
  begin("noteOut", I18N[lang].busyWrite);
  const result = await callJson(ctx, "PUT", path, { title, body }, { label: "PUT /api/notes/:id" });
  const ok = result.status === 200;
  paint("noteOut", ok ? MSGS[lang].noteUpdated(id) : MSGS[lang].notFound(), ok, line("PUT", path, result));
  refreshStats(true);
  noteList();
}

async function noteDelete(id) {
  if (!window.confirm(I18N[lang].confirmNote + " " + id + "?")) return;
  const path = "/api/notes/" + id;
  begin("noteOut", I18N[lang].busyWrite);
  const result = await callJson(ctx, "DELETE", path, undefined, { label: "DELETE /api/notes/:id" });
  const ok = result.status === 204;
  paint("noteOut", ok ? MSGS[lang].noteDeleted(id) : MSGS[lang].notFound(), ok, line("DELETE", path, result));
  refreshStats(true);
  noteList();
}

/* ------------------------------------------------------------------ *
 * Vault
 * ------------------------------------------------------------------ */

const ctx = {
  get token() { return token; },
  trace,
  onHttpError: (status, body) => toast(status + " · " + body, "fail")
};

function renderAuthBadge() {
  const badge = cache("authBadge");
  badge.textContent = protectedMode ? I18N[lang].modeProtected : I18N[lang].modeOpen;
  badge.dataset.tone = protectedMode ? "warn" : "ok";

  const notice = cache("authNotice");
  notice.hidden = protectedMode;
  notice.textContent = I18N[lang].authOpenNotice;
}

async function authGood() {
  token = cache("authToken").value;
  saveToken(token);
  if (!token) { toast(I18N[lang].typePassword); return; }
  const path = "/api/kv/authtest";
  begin("authOut", I18N[lang].busyReq);
  const result = await callJson(ctx, "PUT", path, { value: "ok" }, { label: "PUT /api/kv/:key (auth)" });
  const ok = result.status === 200;
  const human = ok ? (protectedMode ? MSGS[lang].authAccepted() : MSGS[lang].authOpenServer()) : MSGS[lang].authRejected();
  paint("authOut", human, ok, line("PUT", path, result, "X-API-Key: ****"));
  refreshStats(true);
}

async function authWrong() {
  const raw = JSON.stringify({ value: "ok" });
  const headers = { "Content-Type": "application/json", "X-API-Key": I18N[lang].wrongKey };
  begin("authOut", I18N[lang].busyReq);
  const result = await doFetch("PUT", "/api/kv/authtest", raw, headers);
  trace({ method: "PUT", path: "/api/kv/authtest", reqBody: raw, status: result.status,
    ms: result.ms, bytes: result.bytes, respBody: result.text, label: "PUT /api/kv/:key (wrong key)" });
  const denied = result.status === 403;
  paint("authOut", denied ? MSGS[lang].authRejected() : MSGS[lang].authOpenServer(), denied,
    line("PUT", "/api/kv/authtest", result, "X-API-Key: wrong"));
  refreshStats(true);
}

async function authNone() {
  const path = "/api/kv/authtest";
  begin("authOut", I18N[lang].busyReq);
  const result = await callJson(ctx, "PUT", path, { value: "ok" },
    { noAuth: true, label: "PUT /api/kv/:key (no key)" });
  const denied = result.status === 401;
  paint("authOut", denied ? MSGS[lang].authMissing() : MSGS[lang].authOpenServer(),
    denied || result.status === 200, line("PUT", path, result, "no key sent"));
  refreshStats(true);
}

function authForget() {
  token = "";
  cache("authToken").value = "";
  forgetToken();
  toast(I18N[lang].tokenForgotten, "ok");
}

/* ------------------------------------------------------------------ *
 * Echo
 * ------------------------------------------------------------------ */

async function echoSend() {
  const text = cache("echoInput").value;
  begin("echoOut", I18N[lang].busyReq);
  const result = await callJson(ctx, "POST", "/api/echo", { data: text }, { label: "POST /api/echo" });
  let human = MSGS[lang].badRequest();
  if (result.status === 200) {
    try { human = MSGS[lang].echoRoundTrip(text, JSON.parse(result.body).data); }
    catch (e) { human = MSGS[lang].echoRoundTrip(text, result.body); }
  }
  paint("echoOut", human, result.status === 200, line("POST", "/api/echo", result));
}

/* ------------------------------------------------------------------ *
 * Error gallery
 * ------------------------------------------------------------------ */

async function runProbe(kind) {
  const specs = {
    errMissing: ["GET", "/api/kv/never-saved", undefined, "GET /api/kv/:key (missing)"],
    errBadId: ["GET", "/api/notes/abc", undefined, "GET /api/notes/:id (bad id)"],
    errMethod: ["POST", "/health", {}, "POST /health (method)"],
    errPayload: ["PUT", "/api/kv/demo", { wrong: 1 }, "PUT /api/kv/:key (payload)"],
    errTooBig: ["PUT", "/api/kv/big", { value: "x".repeat(70000) }, "PUT /api/kv/:key (oversized)"],
    errRoute: ["GET", "/no-such-route", undefined, "GET /no-such-route"]
  };
  const [method, path, body, label] = specs[kind];
  begin("errOut", I18N[lang].busyReq);
  const result = await callJson(ctx, method, path, body, { label });
  const human = {
    400: MSGS[lang].badRequest(),
    404: MSGS[lang].notFound(),
    405: MSGS[lang].methodNotAllowed(),
    413: MSGS[lang].tooLarge()
  }[result.status] || MSGS[lang].internalError();
  paint("errOut", human, false, line(method, path, result));
}

/* ------------------------------------------------------------------ *
 * Performance
 * ------------------------------------------------------------------ */

function selectPerf(count) {
  perfCount = count;
  document.querySelectorAll("[data-perf]").forEach((btn) => {
    btn.setAttribute("aria-pressed", Number(btn.getAttribute("data-perf")) === count ? "true" : "false");
  });
}

async function perfRun() {
  const count = perfCount;
  begin("perfOut", I18N[lang].busyBench);
  const started = performance.now();
  const results = await Promise.all(
    Array.from({ length: count }, () => fetch("/health").then((r) => r.ok).catch(() => false))
  );
  const total = Math.round(performance.now() - started);
  const failed = results.filter((ok) => !ok).length;
  const average = (total / count).toFixed(1);

  paint("perfOut", MSGS[lang].perf(count, failed, total, average), failed === 0,
    count + "x GET /health  →  HTTP 200 x" + (count - failed) + "  ·  " + total + "ms total  ·  " + average + "ms avg");

  for (let i = 0; i < count; i++) {
    countSession("GET /health (perf)", i < count - failed ? 200 : 500);
  }
  trace({ method: "GET", path: "/health x" + count, reqBody: "", status: failed === 0 ? 200 : 500,
    ms: total, bytes: 0, respBody: MSGS[lang].perf(count, failed, total, average),
    label: "GET /health (perf)" });
  refreshStats(true);
}

/* ------------------------------------------------------------------ *
 * Tech drawer
 * ------------------------------------------------------------------ */

async function rawRun() {
  const method = cache("rawMethod").value;
  const path = cache("rawPath").value.trim() || "/";
  let headers;
  try {
    headers = parseHeaders(cache("rawHeaders").value);
  } catch (error) {
    toast(I18N[lang].badHeader + ": " + error.message, "fail");
    return;
  }
  const raw = cache("rawBody").value;
  if (raw && !headers["Content-Type"] && !headers["content-type"]) {
    headers["Content-Type"] = "application/json";
  }
  const result = await doFetch(method, path, raw, headers);
  trace({ method, path, reqBody: raw, status: result.status, ms: result.ms,
    bytes: result.bytes, respBody: result.text, label: method + " " + path + " (raw)" });
  const ok = result.status >= 200 && result.status < 300;
  toast(line(method, path, result), ok ? "ok" : "fail");
}

function copyCurl() {
  if (!lastCall) { toast(I18N[lang].nothingToCopy); return; }
  const command = buildCurl(lastCall.method, lastCall.path, lastCall.headers, lastCall.reqBody);
  const done = () => toast(I18N[lang].copied, "ok");
  if (navigator.clipboard && navigator.clipboard.writeText) {
    navigator.clipboard.writeText(command).then(done, () => toast(command));
  } else {
    toast(command);
  }
}

function clearStream() {
  history.length = 0;
  session.clear();
  cache("history").textContent = "";
  cache("lastCall").textContent = "";
  renderSession();
}

/* ------------------------------------------------------------------ *
 * Language comparison
 * ------------------------------------------------------------------ */

const KEYWORDS = {
  c: "auto break case const continue default do else enum extern for goto if inline int long register return short signed sizeof static struct switch typedef union unsigned void volatile while",
  python: "and as assert async await break class continue def del elif else except False finally for from global if import in is lambda None nonlocal not or pass raise return True try while with yield",
  go: "break case chan const continue default defer else fallthrough for func go goto if import interface map package range return select struct switch type var nil true false",
  rust: "as async await break const continue crate dyn else enum extern false fn for if impl in let loop match mod move mut pub ref return self Self static struct super trait true type unsafe use where while",
  node: "async await break case catch class const continue default delete do else export extends false finally for from function if import in instanceof let new null of return static super switch this throw true try typeof var void while yield"
};

const API_TOKENS = {
  c: "API_OK API_ERR_INTERNAL API_ERR_NOT_FOUND API_ERR_UNAUTHORIZED API_ERR_FORBIDDEN SQLITE_TRANSIENT SQLITE_DONE SQLITE_ROW sqlite3_prepare_v2 sqlite3_bind_text sqlite3_bind_int64 sqlite3_step sqlite3_finalize sqlite3_column_text sqlite3_changes sqlite3_last_insert_rowid server_send_all"
};

function highlight(code, language) {
  const keywords = new Set((KEYWORDS[language] || "").split(/\s+/).filter(Boolean));
  const api = new Set((API_TOKENS[language] || "").split(/\s+/).filter(Boolean));
  const pattern = /(\/\*[\s\S]*?\*\/|\/\/[^\n]*|"(?:[^"\\]|\\.)*"|'(?:[^'\\]|\\.)*'|\b\d+(?:\.\d+)?\b|[A-Za-z_][A-Za-z0-9_]*)/g;
  let out = "";
  let last = 0;
  let match;

  while ((match = pattern.exec(code)) !== null) {
    out += escapeHtml(code.slice(last, match.index));
    const token = match[0];
    const after = code.slice(pattern.lastIndex, pattern.lastIndex + 1);
    if (token.indexOf("/*") === 0 || token.indexOf("//") === 0) {
      out += '<span class="tok-com">' + escapeHtml(token) + "</span>";
    } else if (token[0] === '"' || token[0] === "'") {
      out += '<span class="tok-str">' + escapeHtml(token) + "</span>";
    } else if (/^\d/.test(token)) {
      out += '<span class="tok-num">' + escapeHtml(token) + "</span>";
    } else if (keywords.has(token)) {
      out += '<span class="tok-kw">' + escapeHtml(token) + "</span>";
    } else if (api.has(token)) {
      out += '<span class="tok-api">' + escapeHtml(token) + "</span>";
    } else if (after === "(") {
      out += '<span class="tok-fn">' + escapeHtml(token) + "</span>";
    } else {
      out += escapeHtml(token);
    }
    last = pattern.lastIndex;
  }
  out += escapeHtml(code.slice(last));
  return out;
}

function codePanel(language, entry, tagText, tagTone, variant) {
  return '<article class="code-panel" data-' + variant + '="true">' +
    '<header class="code-head"><strong>' + IMPLNAME[language] + "</strong>" +
    '<span class="spacer"></span>' +
    '<button class="copy-btn" data-copy="' + language + '">copy</button></header>' +
    '<div class="code-meta"><span class="chip">≈ ' + entry.loc + " loc</span>" +
    '<span class="chip">' + escapeHtml(entry.lib) + "</span>" +
    '<span class="chip" data-tone="' + tagTone + '">' + tagText + "</span></div>" +
    "<pre><code>" + highlight(entry.code, language) + "</code></pre></article>";
}

function renderCompare(host) {
  const key = host.getAttribute("data-fn");
  const data = CMP[key];
  if (!data) return;
  if (!host._selected) host._selected = "c";
  const selected = host._selected;
  const dict = I18N[lang];

  const tabs = ['<div class="tabs" role="group" aria-label="' + escapeHtml(dict.implAria) + '">'];
  IMPL.forEach((language) => {
    tabs.push('<button data-tab="' + language + '" aria-pressed="' +
      (language === selected ? "true" : "false") + '">' + IMPLNAME[language] + "</button>");
  });
  tabs.push("</div>");

  const side = selected !== "c";
  const panels = ['<div class="code-grid" data-side="' + side + '">'];
  panels.push(codePanel("c", data.c, dict.realTag, "ok", "ref"));
  if (side) panels.push(codePanel(selected, data[selected], dict.demoTag, "demo", "cand"));
  panels.push("</div>");

  host.innerHTML = tabs.join("") + panels.join("");

  host.querySelectorAll("[data-tab]").forEach((btn) => {
    btn.addEventListener("click", () => {
      host._selected = btn.getAttribute("data-tab");
      renderCompare(host);
    });
  });
  host.querySelectorAll("[data-copy]").forEach((btn) => {
    btn.addEventListener("click", () => {
      const code = data[btn.getAttribute("data-copy")].code;
      if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(code).then(() => toast(dict.copied, "ok"), () => toast(code.slice(0, 140)));
      } else {
        toast(code.slice(0, 140));
      }
    });
  });
}

function renderMatrix() {
  const active = IMPL.indexOf(currentImpl);
  cache("matrixBody").innerHTML = PIECES.map((piece) => {
    const cells = piece[1].map((value, index) =>
      "<td" + (index === active ? ' class="on"' : "") + ">" + escapeHtml(value) + "</td>").join("");
    return "<tr><td>" + PIECE_L[lang][piece[0]] + "</td>" + cells + "</tr>";
  }).join("");
}

function renderImplNote() {
  cache("implNote").textContent = LANGNOTES[lang][currentImpl];
  document.querySelectorAll("#implTabs button").forEach((btn) => {
    btn.setAttribute("aria-pressed", btn.getAttribute("data-impl") === currentImpl ? "true" : "false");
  });
}

function selectImpl(language) {
  currentImpl = language;
  renderImplNote();
  renderMatrix();
}

/* ------------------------------------------------------------------ *
 * Health + metrics
 * ------------------------------------------------------------------ */

async function refreshStats(silent) {
  try {
    const health = await callJson(ctx, "GET", "/health", undefined, { trace: !silent, label: "GET /health" });
    const metrics = await callJson(ctx, "GET", "/metrics", undefined, { trace: false, label: "GET /metrics" });
    const info = JSON.parse(health.body);
    const stats = JSON.parse(metrics.body);

    cache("beacon").dataset.state = "online";
    cache("healthLine").textContent = I18N[lang].online +
      "  ·  v" + info.version + "  ·  uptime " + info.uptime_s + "s";

    cache("mReq").textContent = stats.http_requests;
    cache("mErr").textContent = stats.http_errors;
    cache("mKv").textContent = stats.kv_count;
    cache("mNotes").textContent = stats.notes_count;
    cache("mConn").textContent = stats.connections_accepted;

    protectedMode = info.auth === "protected";
    renderAuthBadge();
  } catch (error) {
    cache("beacon").dataset.state = "offline";
    cache("healthLine").textContent = I18N[lang].offline;
  }
}

/* ------------------------------------------------------------------ *
 * Wiring
 * ------------------------------------------------------------------ */

const actions = {
  kvSave, kvFetch, kvDelete,
  authGood, authWrong, authNone, authForget,
  echoSend, perfRun, rawRun, copyCurl, clearStream
};

const noteHandlers = { noteCreate, noteList };
const probes = {
  errMissing: () => runProbe("errMissing"),
  errBadId: () => runProbe("errBadId"),
  errMethod: () => runProbe("errMethod"),
  errPayload: () => runProbe("errPayload"),
  errTooBig: () => runProbe("errTooBig"),
  errRoute: () => runProbe("errRoute")
};

function bind() {
  document.querySelectorAll("[data-action]").forEach((button) => {
    button.addEventListener("click", () => {
      const handler = actions[button.getAttribute("data-action")];
      if (handler) busy(button, handler);
    });
  });
  document.querySelectorAll("[data-note]").forEach((button) => {
    button.addEventListener("click", () => {
      const handler = noteHandlers[button.getAttribute("data-note")];
      if (handler) busy(button, handler);
    });
  });
  document.querySelectorAll("[data-err]").forEach((button) => {
    button.addEventListener("click", () => {
      const handler = probes[button.getAttribute("data-err")];
      if (handler) busy(button, handler);
    });
  });
  document.querySelectorAll("[data-perf]").forEach((button) => {
    button.addEventListener("click", () => selectPerf(Number(button.getAttribute("data-perf"))));
  });
  document.querySelectorAll("#langSwitch button").forEach((button) => {
    button.addEventListener("click", () => applyLanguage(button.getAttribute("data-lang")));
  });
  document.querySelectorAll("#implTabs button").forEach((button) => {
    button.addEventListener("click", () => selectImpl(button.getAttribute("data-impl")));
  });
  document.querySelectorAll("details").forEach((details) => {
    const host = details.querySelector("[data-fn]");
    if (!host) return;
    details.addEventListener("toggle", () => {
      if (details.open && !host._selected) renderCompare(host);
    });
  });

  cache("authToken").value = token || I18N[lang].defaultToken;
  selectPerf(10);
}

function start() {
  bind();
  applyLanguage(lang);
  renderImplNote();
  refreshStats(false);
  setInterval(() => refreshStats(true), 5000);
}

if (document.readyState === "loading") {
  document.addEventListener("DOMContentLoaded", start);
} else {
  start();
}
