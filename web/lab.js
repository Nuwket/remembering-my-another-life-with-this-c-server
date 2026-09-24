/**
 * lab.js - Lab behaviors. The real C backend does the work; this file only
 * translates user intent into real requests and real responses into verdicts.
 */
import { I18N, MSGS, TIPS, HOW, detectLang } from "./i18n.js";
import { CMP, IMPL, IMPLNAME, LANGNOTES, PIECES, PIECE_L } from "./compare.js";
import { callJson, doFetch, buildCurl, parseHeaders, explain, loadToken, saveToken, forgetToken } from "./api.js";

let LANG = detectLang();
let token = loadToken();
let serverOpen = true;
let lastEntry = null;
let currentImpl = "c";
let perfCount = 10;

const history = [];
const session = {};
const $ = (id) => document.getElementById(id);

/* ---------------- i18n rendering ---------------- */

function applyLanguage(code) {
  if (!I18N[code]) code = "en";
  LANG = code;
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
  document.querySelectorAll("#langBtns button").forEach((btn) => {
    const active = btn.getAttribute("data-lang") === code;
    btn.classList.toggle("active", active);
    btn.setAttribute("aria-pressed", active ? "true" : "false");
  });

  try { localStorage.setItem("capi-lang", code); } catch (e) { /* private mode */ }

  document.querySelectorAll("[data-fn]").forEach((div) => {
    if (div._sel) renderCompare(div);
  });
  renderImplMatrix();
  updateAuthNotice();
  if ($("dot").className.indexOf("ok") < 0) refreshHealth(true);
}

/* ---------------- toasts ---------------- */

function toast(message, ok) {
  const box = $("toast");
  const item = document.createElement("div");
  if (ok) item.className = "ok";
  item.textContent = message;
  box.appendChild(item);
  setTimeout(() => item.remove(), 6000);
}

/* ---------------- results ---------------- */

function showResult(outId, techId, human, ok, tech) {
  const el = $(outId);
  el.textContent = human;
  el.classList.remove("good", "fail");
  el.classList.add(ok ? "good" : "fail");
  if (techId) $(techId).textContent = tech;
}

function techLine(method, path, result) {
  return method + " " + path + " → HTTP " + result.status + " · " + result.ms + "ms · " + result.bytes + " bytes";
}

/* ---------------- tracing + session ---------------- */

function recordSession(label, status) {
  if (!label) return;
  const bucket = status >= 200 && status < 300 ? "ok" : status >= 400 && status < 500 ? "c4" : "c5";
  if (!session[label]) session[label] = { ok: 0, c4: 0, c5: 0 };
  session[label][bucket] += 1;
  renderSessionTable();
}

function renderSessionTable() {
  const rows = Object.keys(session).sort().map((label) => {
    const counts = session[label];
    const total = counts.ok + counts.c4 + counts.c5;
    return "<tr><td>" + label + "</td><td>" + counts.ok + "</td><td>" + counts.c4 +
      "</td><td>" + counts.c5 + "</td><td>" + total + "</td></tr>";
  });
  $("sessBody").innerHTML = rows.length ? rows.join("") : '<tr><td colspan="5">—</td></tr>';
}

function trace(entry) {
  lastEntry = entry;
  history.unshift(entry);
  if (history.length > 15) history.pop();

  const last = $("devLast");
  last.textContent =
    entry.method + " " + entry.path + " → " + entry.status + " · " + entry.ms + "ms · " + entry.bytes + " bytes\n" +
    "-- request --\n" + (entry.reqBody || "(empty)") + "\n-- response --\n" + entry.respBody;
  last.scrollTop = last.scrollHeight;
  $("devExplainTxt").textContent = explain(entry.status, entry.respBody);

  const stamp = new Date().toLocaleTimeString();
  $("devHist").textContent = history
    .map((item) => stamp + "  " + item.method + " " + item.path + " → " + item.status + " · " + item.ms + "ms")
    .join("\n");

  recordSession(entry.label || entry.method + " " + entry.path, entry.status);
}

const ctx = {
  get token() { return token; },
  set token(value) { token = value; },
  trace,
  onHttpError: (status, body) => toast(status + " " + body)
};

/* ---------------- button helper (loading state) ---------------- */

async function withButton(button, action) {
  if (button.disabled) return;
  button.disabled = true;
  const label = button.textContent;
  button.textContent = "…";
  button.setAttribute("aria-busy", "true");
  try {
    await action();
  } catch (error) {
    toast("network error: " + error);
  } finally {
    button.disabled = false;
    button.textContent = label;
    button.removeAttribute("aria-busy");
  }
}

/* ---------------- 💾 save / fetch / delete ---------------- */

const kvValues = () => ({
  key: $("kvKey").value.trim(),
  value: $("kvVal").value
});

async function kvSave(button) {
  const values = kvValues();
  const path = "/api/kv/" + encodeURIComponent(values.key);
  const result = await callJson(ctx, "PUT", path, { value: values.value }, { label: "PUT /api/kv/:key" });
  const ok = result.status >= 200 && result.status < 300;
  showResult("kvResult", "kvTech", ok ? MSGS[LANG].saved(values.key, values.value) : MSGS[LANG].err400(),
    ok, techLine("PUT", path, result));
  refreshHealth(true);
}

async function kvFetch(button) {
  const values = kvValues();
  const path = "/api/kv/" + encodeURIComponent(values.key);
  const result = await callJson(ctx, "GET", path, undefined, { label: "GET /api/kv/:key" });
  let human = MSGS[LANG].err404();
  if (result.status === 200) {
    try { human = MSGS[LANG].fetched(JSON.parse(result.body).value); }
    catch (e) { human = MSGS[LANG].fetched(result.body); }
  }
  showResult("kvResult", "kvTech", human, result.status === 200, techLine("GET", path, result));
}

async function kvDelete(button) {
  const values = kvValues();
  if (!confirm(I18N[LANG].confirmDeleteKv + " " + values.key + "?")) return;
  const path = "/api/kv/" + encodeURIComponent(values.key);
  const result = await callJson(ctx, "DELETE", path, undefined, { label: "DELETE /api/kv/:key" });
  const ok = result.status === 204;
  showResult("kvResult", "kvTech", ok ? MSGS[LANG].deleted() : MSGS[LANG].err404(), ok,
    techLine("DELETE", path, result));
  refreshHealth(true);
}

/* ---------------- 📝 notes ---------------- */

const noteValues = () => ({ title: $("nTitle").value, body: $("nBody").value });

function renderNoteCards(items) {
  const host = $("notesLive");
  host.textContent = "";
  items.forEach((note) => {
    const card = document.createElement("div");
    card.className = "notecard";

    const title = document.createElement("b");
    title.textContent = "#" + note.id + " " + note.title;

    const body = document.createElement("p");
    body.textContent = note.body;

    const ops = document.createElement("div");
    ops.className = "ops";

    const edit = document.createElement("button");
    edit.className = "ghost";
    edit.textContent = I18N[LANG].editNote;
    edit.setAttribute("aria-label", I18N[LANG].editNote + " " + note.id);
    edit.addEventListener("click", () => {
      $("nTitle").value = note.title;
      $("nBody").value = note.body;
      noteUpdate(note.id);
    });

    const remove = document.createElement("button");
    remove.className = "ghost";
    remove.textContent = I18N[LANG].deleteNote;
    remove.setAttribute("aria-label", I18N[LANG].deleteNote + " " + note.id);
    remove.addEventListener("click", () => noteDelete(note.id));

    ops.appendChild(edit);
    ops.appendChild(remove);
    card.appendChild(title);
    card.appendChild(body);
    card.appendChild(ops);
    host.appendChild(card);
  });
}

async function noteCreate(button) {
  const values = noteValues();
  const result = await callJson(ctx, "POST", "/api/notes", { title: values.title, body: values.body },
    { label: "POST /api/notes" });
  let human = MSGS[LANG].err400();
  if (result.status === 201) {
    try { human = MSGS[LANG].noteMade(JSON.parse(result.body).id); }
    catch (e) { human = MSGS[LANG].noteMade("?"); }
  }
  showResult("nResult", "nTech", human, result.status === 201, techLine("POST", "/api/notes", result));
  refreshHealth(true);
  noteList();
}

async function noteList(button) {
  const path = "/api/notes?limit=50";
  const result = await callJson(ctx, "GET", path, undefined, { label: "GET /api/notes" });
  let human = MSGS[LANG].err400();
  let items = [];
  if (result.status === 200) {
    try {
      const parsed = JSON.parse(result.body);
      items = parsed.items || [];
      human = MSGS[LANG].notesListed(parsed.total !== undefined ? parsed.total : items.length);
    } catch (e) { human = MSGS[LANG].err400(); }
  }
  showResult("nResult", "nTech", human, result.status === 200, techLine("GET", path, result));
  renderNoteCards(items);
}

async function noteUpdate(id) {
  const values = noteValues();
  const path = "/api/notes/" + id;
  const result = await callJson(ctx, "PUT", path, { title: values.title, body: values.body },
    { label: "PUT /api/notes/:id" });
  const ok = result.status === 200;
  showResult("nResult", "nTech", ok ? MSGS[LANG].noteUpdated(id) : MSGS[LANG].err404(), ok,
    techLine("PUT", path, result));
  refreshHealth(true);
  noteList();
}

async function noteDelete(id) {
  if (!confirm(I18N[LANG].confirmDeleteNote + " " + id + "?")) return;
  const path = "/api/notes/" + id;
  const result = await callJson(ctx, "DELETE", path, undefined, { label: "DELETE /api/notes/:id" });
  const ok = result.status === 204;
  showResult("nResult", "nTech", ok ? MSGS[LANG].noteGone(id) : MSGS[LANG].err404(), ok,
    techLine("DELETE", path, result));
  refreshHealth(true);
  noteList();
}

/* ---------------- 🔐 vault ---------------- */

function rememberPassword() {
  token = $("apiPass").value;
  saveToken(token);
  return token;
}

function updateAuthNotice() {
  const notice = $("authNotice");
  if (serverOpen) {
    notice.hidden = false;
    notice.textContent = I18N[LANG].openNote;
  } else {
    notice.hidden = true;
  }
  const badge = $("authMode");
  badge.textContent = serverOpen ? I18N[LANG].modeOpen : I18N[LANG].modeProtected;
  badge.className = "badge " + (serverOpen ? "open" : "protected");
}

async function authGood(button) {
  if (!rememberPassword()) { toast(I18N[LANG].typePassword); return; }
  const path = "/api/kv/authtest";
  const result = await callJson(ctx, "PUT", path, { value: "ok" }, { label: "PUT /api/kv/:key (auth)" });
  const human = result.status === 200
    ? (serverOpen ? MSGS[LANG].authOpen() : MSGS[LANG].authOk())
    : MSGS[LANG].authBad();
  showResult("authResult", "authTech", human, result.status === 200,
    techLine("PUT", path, result) + " · X-API-Key: ••••");
  refreshHealth(true);
}

async function authWrong(button) {
  const raw = JSON.stringify({ value: "ok" });
  const headers = { "Content-Type": "application/json", "X-API-Key": I18N[LANG].wrongKey };
  const result = await doFetch("PUT", "/api/kv/authtest", raw, headers);
  trace({ method: "PUT", path: "/api/kv/authtest", reqBody: raw, headers, status: result.status,
    ms: result.ms, bytes: result.bytes, respBody: result.text, label: "PUT /api/kv/:key (wrong key)" });
  const human = result.status === 403 ? MSGS[LANG].authBad() : MSGS[LANG].authOpen();
  showResult("authResult", "authTech", human, result.status === 403,
    techLine("PUT", "/api/kv/authtest", result) + " · X-API-Key: " + I18N[LANG].wrongKey);
  refreshHealth(true);
}

async function authNone(button) {
  rememberPassword();
  const path = "/api/kv/authtest";
  const result = await callJson(ctx, "PUT", path, { value: "ok" },
    { noAuth: true, label: "PUT /api/kv/:key (no key)" });
  const human = result.status === 401 ? MSGS[LANG].authNone() : MSGS[LANG].authOpen();
  showResult("authResult", "authTech", human, result.status === 401 || (serverOpen && result.status === 200),
    techLine("PUT", path, result) + " · no key sent");
  refreshHealth(true);
}

function forgetPassword() {
  token = "";
  $("apiPass").value = "";
  forgetToken();
  toast(I18N[LANG].tokenForgotten, true);
}

/* ---------------- 📡 echo ---------------- */

async function echoSend(button) {
  const text = $("echoIn").value;
  const result = await callJson(ctx, "POST", "/api/echo", { data: text }, { label: "POST /api/echo" });
  let human = MSGS[LANG].err400();
  if (result.status === 200) {
    try { human = MSGS[LANG].echoGot(text, JSON.parse(result.body).data); }
    catch (e) { human = MSGS[LANG].echoGot(text, result.body); }
  }
  showResult("echoResult", "echoTech", human, result.status === 200, techLine("POST", "/api/echo", result));
}

/* ---------------- 💥 error gallery ---------------- */

async function errMissing() {
  const result = await callJson(ctx, "PUT", "/api/kv/demo", { nope: 1 }, { label: "PUT /api/kv/:key (missing field)" });
  showResult("errResult", "errTech", MSGS[LANG].err400(), false, techLine("PUT", "/api/kv/demo", result));
}

async function errBadId() {
  const result = await callJson(ctx, "GET", "/api/notes/abc", undefined, { label: "GET /api/notes/:id (bad id)" });
  showResult("errResult", "errTech", MSGS[LANG].err400(), false, techLine("GET", "/api/notes/abc", result));
}

async function errWrongMethod() {
  const result = await callJson(ctx, "POST", "/health", {}, { label: "POST /health (method)" });
  showResult("errResult", "errTech", MSGS[LANG].err405(), false, techLine("POST", "/health", result));
}

async function errTooBig() {
  const result = await callJson(ctx, "PUT", "/api/kv/big", { value: "x".repeat(70000) },
    { label: "PUT /api/kv/:key (oversized)" });
  showResult("errResult", "errTech", MSGS[LANG].err413(), false, techLine("PUT", "/api/kv/big", result));
}

async function errUnknownRoute() {
  const result = await callJson(ctx, "GET", "/lugar-nenhum", undefined, { label: "GET /unknown (route)" });
  showResult("errResult", "errTech", MSGS[LANG].err404(), false, techLine("GET", "/lugar-nenhum", result));
}

async function errMissingValue() {
  const result = await callJson(ctx, "GET", "/api/kv/nunca-existiu", undefined,
    { label: "GET /api/kv/:key (missing value)" });
  showResult("errResult", "errTech", MSGS[LANG].err404(), false,
    techLine("GET", "/api/kv/nunca-existiu", result));
}

async function errWrongKey() {
  const raw = JSON.stringify({ value: "ok" });
  const headers = { "Content-Type": "application/json", "X-API-Key": I18N[LANG].wrongKey };
  const result = await doFetch("PUT", "/api/kv/authtest", raw, headers);
  trace({ method: "PUT", path: "/api/kv/authtest", reqBody: raw, headers, status: result.status,
    ms: result.ms, bytes: result.bytes, respBody: result.text, label: "PUT /api/kv/:key (wrong key)" });
  const human = result.status === 403 ? MSGS[LANG].err403() : MSGS[LANG].authOpen();
  showResult("errResult", "errTech", human, false, techLine("PUT", "/api/kv/authtest", result));
}

/* ---------------- ⚡ performance ---------------- */

function selectPerfCount(count) {
  perfCount = count;
  document.querySelectorAll("[data-perf]").forEach((btn) => {
    const value = parseInt(btn.getAttribute("data-perf"), 10);
    btn.setAttribute("aria-pressed", value === count ? "true" : "false");
  });
}

async function runPerf(button) {
  const count = perfCount;
  const started = performance.now();
  const results = await Promise.all(Array.from({ length: count }, () =>
    fetch("/health").then((r) => ({ ok: r.ok })).catch(() => ({ ok: false }))));
  const total = Math.round(performance.now() - started);
  const errors = results.filter((item) => !item.ok).length;
  const average = (total / count).toFixed(1);

  showResult("perfResult", "perfTech", MSGS[LANG].perf(count, errors, total, average), errors === 0,
    count + "× GET /health · " + total + "ms total · " + average + "ms avg");
  for (let i = 0; i < count; i++) recordSession("GET /health (perf)", i < count - errors ? 200 : 500);
  trace({ method: "GET", path: "/health ×" + count, reqBody: "", status: errors === 0 ? 200 : 500,
    ms: total, bytes: 0, respBody: MSGS[LANG].perf(count, errors, total, average),
    label: "GET /health (perf)" });
  refreshHealth(true);
}

/* ---------------- 🛠 tech mode ---------------- */

function copyCurl() {
  if (!lastEntry) { toast(I18N[LANG].nothingToCopy); return; }
  const command = buildCurl(lastEntry.method, lastEntry.path, lastEntry.headers, lastEntry.reqBody);
  const done = () => toast(I18N[LANG].copied, true);
  if (navigator.clipboard && navigator.clipboard.writeText) {
    navigator.clipboard.writeText(command).then(done, () => toast(command));
  } else {
    toast(command);
  }
}

function clearHistory() {
  history.length = 0;
  $("devHist").textContent = "—";
}

async function composerRun(button) {
  const method = $("cMethod").value;
  const path = $("cPath").value.trim() || "/";
  let headers;
  try {
    headers = parseHeaders($("cHeaders").value);
  } catch (error) {
    toast("400 local: " + error.message);
    return;
  }
  const raw = $("cBody").value;
  if (raw && !headers["Content-Type"] && !headers["content-type"]) headers["Content-Type"] = "application/json";
  const result = await doFetch(method, path, raw, headers);
  trace({ method, path, reqBody: raw, headers, status: result.status, ms: result.ms,
    bytes: result.bytes, respBody: result.text, label: method + " " + path + " (composer)" });
  if (result.status < 200 || result.status >= 300) toast(result.status + " " + result.text.slice(0, 200));
  else toast(result.status + " · " + result.ms + "ms · " + result.bytes + " bytes", true);
}

/* ---------------- 🌎 language comparison ---------------- */

function escapeCode(code) {
  return code.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
}

/* Lightweight syntax highlighting: one pass, escaped per token, never twice. */
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

function highlight(code, lang) {
  const keywords = new Set((KEYWORDS[lang] || "").split(/\s+/).filter(Boolean));
  const api = new Set((API_TOKENS[lang] || "").split(/\s+/).filter(Boolean));
  const pattern = /(\/\*[\s\S]*?\*\/|\/\/[^\n]*|"(?:[^"\\]|\\.)*"|'(?:[^'\\]|\\.)*'|\b\d+(?:\.\d+)?\b|[A-Za-z_][A-Za-z0-9_]*)/g;
  let out = "";
  let last = 0;
  let match;
  while ((match = pattern.exec(code)) !== null) {
    out += escapeCode(code.slice(last, match.index));
    const token = match[0];
    const after = code.slice(pattern.lastIndex, pattern.lastIndex + 1);
    if (token.indexOf("/*") === 0 || token.indexOf("//") === 0) {
      out += '<span class="tok-com">' + escapeCode(token) + "</span>";
    } else if (token[0] === '"' || token[0] === "'") {
      out += '<span class="tok-str">' + escapeCode(token) + "</span>";
    } else if (/^\d/.test(token)) {
      out += '<span class="tok-num">' + escapeCode(token) + "</span>";
    } else if (keywords.has(token)) {
      out += '<span class="tok-kw">' + escapeCode(token) + "</span>";
    } else if (api.has(token)) {
      out += '<span class="tok-api">' + escapeCode(token) + "</span>";
    } else if (after === "(") {
      out += '<span class="tok-fn">' + escapeCode(token) + "</span>";
    } else {
      out += escapeCode(token);
    }
    last = pattern.lastIndex;
  }
  out += escapeCode(code.slice(last));
  return out;
}

function comparePane(lang, entry, tagText, tagClass, variant) {
  return '<div class="cmppane ' + variant + '">' +
    '<div class="cmp-head"><h4>' + IMPLNAME[lang] + "</h4>" +
    '<span class="spacer"></span>' +
    '<button class="copy" data-copy="' + lang + '" title="Copy code">⧉ copy</button></div>' +
    '<div class="cmp-meta"><span class="chip">≈ ' + entry.loc + " loc</span>" +
    '<span class="chip">via ' + escapeCode(entry.lib) + "</span>" +
    '<span class="tag ' + tagClass + '">' + tagText + "</span></div>" +
    "<pre><code>" + highlight(entry.code, lang) + "</code></pre></div>";
}

function renderCompare(host) {
  const key = host.getAttribute("data-fn");
  const data = CMP[key];
  if (!data) return;
  if (!host._sel) host._sel = "c";
  const selected = host._sel;
  const dict = I18N[LANG];

  let html = '<div class="cmptabs" role="group" aria-label="' + dict.cmpAria + '">';
  IMPL.forEach((lang) => {
    const active = lang === selected;
    html += '<button class="' + (active ? "active" : "") + '" data-impl-tab="' + lang + '"' +
      ' aria-pressed="' + (active ? "true" : "false") + '">' + IMPLNAME[lang] + "</button>";
  });
  html += '</div><div class="cmpgrid' + (selected !== "c" ? " side" : "") + '">';
  html += comparePane("c", data.c, dict.realTag, "real", " is-reference");
  if (selected !== "c") {
    html += comparePane(selected, data[selected], dict.demoTag, "demo", " is-candidate");
  }
  html += "</div>";
  host.innerHTML = html;

  host.querySelectorAll("[data-impl-tab]").forEach((btn) => {
    btn.addEventListener("click", () => {
      host._sel = btn.getAttribute("data-impl-tab");
      renderCompare(host);
    });
  });
  host.querySelectorAll("[data-copy]").forEach((btn) => {
    btn.addEventListener("click", () => {
      const code = data[btn.getAttribute("data-copy")].code;
      if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(code).then(() => toast(dict.copied, true), () => toast(code.slice(0, 120)));
      } else {
        toast(code.slice(0, 120));
      }
    });
  });
}

function renderImplMatrix() {
  $("langNote").textContent = LANGNOTES[LANG][currentImpl];
  document.querySelectorAll("#langBtns2 button").forEach((btn) => {
    const active = btn.getAttribute("data-impl") === currentImpl;
    btn.classList.toggle("ghost", !active);
    btn.classList.toggle("act", active);
    btn.setAttribute("aria-pressed", active ? "true" : "false");
  });
  const active = IMPL.indexOf(currentImpl);
  const rows = PIECES.map((piece) => {
    const cells = piece[1].map((value, index) =>
      "<td" + (index === active ? ' class="col-active"' : "") + ">" + escapeCode(value) + "</td>").join("");
    return "<tr><td>" + PIECE_L[LANG][piece[0]] + "</td>" + cells + "</tr>";
  });
  $("pieceBody").innerHTML = rows.join("");
}

function selectImpl(lang) {
  currentImpl = lang;
  renderImplMatrix();
}

/* ---------------- health + metrics ---------------- */

async function refreshHealth(silent) {
  try {
    const health = await callJson(ctx, "GET", "/health", undefined, { trace: !silent, label: "GET /health" });
    const metrics = await callJson(ctx, "GET", "/metrics", undefined, { trace: false, label: "GET /metrics" });
    const healthData = JSON.parse(health.body);
    const metricsData = JSON.parse(metrics.body);

    $("dot").className = "dot ok";
    $("healthLine").textContent = "version " + healthData.version + " · uptime " + healthData.uptime_s + "s";
    $("mReq").textContent = metricsData.http_requests;
    $("mErr").textContent = metricsData.http_errors;
    $("mKv").textContent = metricsData.kv_count;
    $("mNotes").textContent = metricsData.notes_count;
    $("mConn").textContent = metricsData.connections_accepted;
    serverOpen = healthData.auth !== "protected";
    updateAuthNotice();
  } catch (error) {
    $("dot").className = "dot bad";
    $("healthLine").textContent = I18N[LANG].unreachable + ": " + error;
  }
}

/* ---------------- wiring ---------------- */

function bind() {
  document.querySelectorAll("[data-action]").forEach((button) => {
    button.addEventListener("click", () => {
      const action = button.getAttribute("data-action");
      const handler = actions[action];
      if (!handler) return;
      withButton(button, () => handler(button));
    });
  });
  document.querySelectorAll("[data-note-action]").forEach((button) => {
    button.addEventListener("click", () => {
      const action = button.getAttribute("data-note-action");
      const handler = noteActions[action];
      if (!handler) return;
      withButton(button, () => handler(button));
    });
  });
  document.querySelectorAll("[data-err]").forEach((button) => {
    button.addEventListener("click", () => withButton(button, () => errorActions[button.getAttribute("data-err")]()));
  });
  document.querySelectorAll("[data-perf]").forEach((button) => {
    button.addEventListener("click", () => selectPerfCount(parseInt(button.getAttribute("data-perf"), 10)));
  });
  document.querySelectorAll("#langBtns button").forEach((button) => {
    button.addEventListener("click", () => applyLanguage(button.getAttribute("data-lang")));
  });
  document.querySelectorAll("#langBtns2 button").forEach((button) => {
    button.addEventListener("click", () => selectImpl(button.getAttribute("data-impl")));
  });
  document.querySelectorAll("details.cmp").forEach((details) => {
    details.addEventListener("toggle", () => {
      if (details.open && !details._done) {
        details._done = true;
        renderCompare(details.querySelector("[data-fn]"));
      }
    });
  });
  $("apiPass").value = token || "demo";
  $("healthLine").textContent = I18N[detectLang()].probing;
  selectPerfCount(10);
}

const actions = {
  kvSave, kvFetch, kvDelete, authGood, authWrong, authNone, echoSend, runPerf, composerRun,
  copyCurl, clearHistory, forgetPassword
};

const noteActions = { noteCreate, noteList };

const errorActions = {
  errMissingValue, errWrongKey, errMissing, errWrongMethod, errTooBig, errUnknownRoute
};

function start() {
  bind();
  applyLanguage(LANG);
  refreshHealth(false);
  setInterval(() => refreshHealth(true), 5000);
}

if (document.readyState === "loading") {
  document.addEventListener("DOMContentLoaded", start);
} else {
  start();
}
