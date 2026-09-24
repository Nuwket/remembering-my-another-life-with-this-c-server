/**
 * compare.js - Same task, five languages.
 * C entries are real, shortened excerpts from this server's src/ tree.
 * Every other entry is an equivalent example, clearly labeled as NOT executed.
 */
export const IMPL = ["c", "python", "go", "rust", "node"];
export const IMPLNAME = { c: "C", python: "Python", go: "Go", rust: "Rust", node: "Node.js" };

export const CMP = {
  save: {
    c: {
      loc: "12", lib: "libsqlite3 (C API)", real: true,
      code: `/* real code from src/store_sqlite.c */
sqlite3_prepare_v2(db,
  "INSERT INTO kv(key,value,updated_at) VALUES(?,?,?) "
  "ON CONFLICT(key) DO UPDATE SET value=excluded.value;",
  -1, &stmt, NULL);
sqlite3_bind_text(stmt, 1, "demo", -1, SQLITE_TRANSIENT);
sqlite3_bind_text(stmt, 2, "funcionando", -1, SQLITE_TRANSIENT);
sqlite3_bind_int64(stmt, 3, (sqlite3_int64)time(NULL));
if (sqlite3_step(stmt) != SQLITE_DONE) return API_ERR_INTERNAL;
sqlite3_finalize(stmt);`
    },
    python: {
      loc: "9", lib: "urllib (stdlib)",
      code: `import json, urllib.request  # stdlib only
body = json.dumps({"value": "funcionando"}).encode()
req = urllib.request.Request("http://127.0.0.1:8080/api/kv/demo",
    data=body, method="PUT",
    headers={"Content-Type": "application/json", "X-API-Key": KEY})
try:
    with urllib.request.urlopen(req, timeout=5) as r:
        print(r.status, r.read().decode())  # 200
except urllib.error.HTTPError as e:
    print("rejected:", e.code)  # errors are exceptions`
    },
    go: {
      loc: "11", lib: "net/http (stdlib)",
      code: `import ("bytes"; "encoding/json"; "fmt"; "net/http")  // stdlib
body, _ := json.Marshal(map[string]string{"value": "funcionando"})
req, _ := http.NewRequest("PUT", "http://127.0.0.1:8080/api/kv/demo",
    bytes.NewReader(body))
req.Header.Set("Content-Type", "application/json")
req.Header.Set("X-API-Key", key)
resp, err := http.DefaultClient.Do(req)  // errors are values
if err != nil { log.Fatal(err) }
defer resp.Body.Close()
fmt.Println(resp.Status)  // 200 OK`
    },
    rust: {
      loc: "9", lib: "reqwest + serde_json (crates)",
      code: `// crates: reqwest (blocking), serde_json
let res = reqwest::blocking::Client::new()
    .put("http://127.0.0.1:8080/api/kv/demo")
    .header("Content-Type", "application/json")
    .header("X-Api-Key", &key)
    .json(&serde_json::json!({"value": "funcionando"}))
    .send()?;  // Result must be handled
println!("{}", res.status());  // 200 OK`
    },
    node: {
      loc: "8", lib: "fetch (built-in 18+)",
      code: `// global fetch, Node 18+ — no packages
const res = await fetch("http://127.0.0.1:8080/api/kv/demo", {
  method: "PUT",
  headers: { "Content-Type": "application/json", "X-API-Key": key },
  body: JSON.stringify({ value: "funcionando" }),
});
console.log(res.status, await res.text());  // rejections throw`
    }
  },
  get: {
    c: {
      loc: "9", lib: "libsqlite3 (C API)", real: true,
      code: `/* real code from src/store_sqlite.c */
sqlite3_prepare_v2(db, "SELECT value FROM kv WHERE key=?;",
    -1, &stmt, NULL);
sqlite3_bind_text(stmt, 1, "demo", -1, SQLITE_TRANSIENT);
rc = sqlite3_step(stmt);
if (rc == SQLITE_ROW) value = sqlite3_column_text(stmt, 0);
else if (rc == SQLITE_DONE) return API_ERR_NOT_FOUND;
sqlite3_finalize(stmt);`
    },
    python: {
      loc: "6", lib: "urllib (stdlib)",
      code: `try:
    with urllib.request.urlopen(
            "http://127.0.0.1:8080/api/kv/demo", timeout=5) as r:
        print(r.status, r.read().decode())  # 200
except urllib.error.HTTPError as e:
    print("missing:" if e.code == 404 else "error:", e.code)`
    },
    go: {
      loc: "8", lib: "net/http (stdlib)",
      code: `resp, err := http.Get("http://127.0.0.1:8080/api/kv/demo")
if err != nil { log.Fatal(err) }
defer resp.Body.Close()
if resp.StatusCode == 404 { fmt.Println("missing") }`
    },
    rust: {
      loc: "6", lib: "reqwest (crate)",
      code: `let res = reqwest::blocking::get(
    "http://127.0.0.1:8080/api/kv/demo")?;
if res.status().as_u16() == 404 { println!("missing"); }`
    },
    node: {
      loc: "5", lib: "fetch (built-in 18+)",
      code: `const res = await fetch("http://127.0.0.1:8080/api/kv/demo");
console.log(res.status === 404 ? "missing" : await res.text());`
    }
  },
  del: {
    c: {
      loc: "8", lib: "libsqlite3 (C API)", real: true,
      code: `/* real code from src/store_sqlite.c */
sqlite3_prepare_v2(db, "DELETE FROM kv WHERE key=?;",
    -1, &stmt, NULL);
sqlite3_bind_text(stmt, 1, "demo", -1, SQLITE_TRANSIENT);
rc = sqlite3_step(stmt);
changes = sqlite3_changes(db);  /* 0 = nothing deleted */
sqlite3_finalize(stmt);
return changes > 0 ? API_OK : API_ERR_NOT_FOUND;`
    },
    python: {
      loc: "6", lib: "urllib (stdlib)",
      code: `req = urllib.request.Request("http://127.0.0.1:8080/api/kv/demo",
    method="DELETE")
with urllib.request.urlopen(req, timeout=5) as r:
    print(r.status)  # 204 = deleted, nothing to read`
    },
    go: {
      loc: "7", lib: "net/http (stdlib)",
      code: `req, _ := http.NewRequest("DELETE",
    "http://127.0.0.1:8080/api/kv/demo", nil)
resp, err := http.DefaultClient.Do(req)
if err != nil { log.Fatal(err) }
defer resp.Body.Close()  // 204 has no body`
    },
    rust: {
      loc: "6", lib: "reqwest (crate)",
      code: `let res = reqwest::blocking::Client::new()
    .delete("http://127.0.0.1:8080/api/kv/demo").send()?;
println!("{}", res.status());  // 204 No Content`
    },
    node: {
      loc: "5", lib: "fetch (built-in 18+)",
      code: `const res = await fetch("http://127.0.0.1:8080/api/kv/demo",
    { method: "DELETE" });
console.log(res.status);  // 204`
    }
  },
  note: {
    c: {
      loc: "10", lib: "libsqlite3 (C API)", real: true,
      code: `/* real code from src/store_sqlite.c */
sqlite3_prepare_v2(db,
  "INSERT INTO notes(title,body,created_at,updated_at) "
  "VALUES(?,?,?,?);", -1, &stmt, NULL);
sqlite3_bind_text(stmt, 1, title, -1, SQLITE_TRANSIENT);
sqlite3_bind_text(stmt, 2, body, -1, SQLITE_TRANSIENT);
sqlite3_step(stmt);
id = sqlite3_last_insert_rowid(db);  /* auto-increment id */
sqlite3_finalize(stmt);`
    },
    python: {
      loc: "7", lib: "urllib (stdlib)",
      code: `body = json.dumps({"title": "hello", "body": "..."}).encode()
req = urllib.request.Request("http://127.0.0.1:8080/api/notes",
    data=body, method="POST",
    headers={"Content-Type": "application/json", "X-API-Key": KEY})
with urllib.request.urlopen(req, timeout=5) as r:
    print(r.status, json.loads(r.read())["id"])  # 201 + id`
    },
    go: {
      loc: "10", lib: "net/http (stdlib)",
      code: `body, _ := json.Marshal(map[string]string{
    "title": "hello", "body": "..."})
req, _ := http.NewRequest("POST", "http://127.0.0.1:8080/api/notes",
    bytes.NewReader(body))
req.Header.Set("Content-Type", "application/json")
req.Header.Set("X-API-Key", key)
resp, err := http.DefaultClient.Do(req)  // 201 Created
if err != nil { log.Fatal(err) }
defer resp.Body.Close()`
    },
    rust: {
      loc: "9", lib: "reqwest + serde_json (crates)",
      code: `let res = reqwest::blocking::Client::new()
    .post("http://127.0.0.1:8080/api/notes")
    .header("X-Api-Key", &key)
    .json(&serde_json::json!({"title": "hello", "body": "..."}))
    .send()?;  // 201 Created + {"id": N, ...}`
    },
    node: {
      loc: "8", lib: "fetch (built-in 18+)",
      code: `const res = await fetch("http://127.0.0.1:8080/api/notes", {
  method: "POST",
  headers: { "Content-Type": "application/json", "X-API-Key": key },
  body: JSON.stringify({ title: "hello", body: "..." }),
});
console.log(res.status, (await res.json()).id);  // 201`
    }
  },
  echo: {
    c: {
      loc: "6", lib: "POSIX sockets", real: true,
      code: `/* real code from src/server.c */
ssize_t n = recv(fd, buf, sizeof(buf), 0);  /* partial reads */
if (n == 0) return;  /* peer closed */
server_send_all(fd, buf, (size_t)n);  /* loop until all sent */`
    },
    python: {
      loc: "6", lib: "urllib (stdlib)",
      code: `body = json.dumps({"data": "hello"}).encode()
req = urllib.request.Request("http://127.0.0.1:8080/api/echo",
    data=body, method="POST",
    headers={"Content-Type": "application/json"})
with urllib.request.urlopen(req, timeout=5) as r:
    print(json.loads(r.read())["data"])  # same bytes back`
    },
    go: {
      loc: "8", lib: "net/http (stdlib)",
      code: `body, _ := json.Marshal(map[string]string{"data": "hello"})
resp, err := http.Post("http://127.0.0.1:8080/api/echo",
    "application/json", bytes.NewReader(body))
if err != nil { log.Fatal(err) }
defer resp.Body.Close()  // server echoes the bytes`
    },
    rust: {
      loc: "8", lib: "reqwest + serde_json (crates)",
      code: `let res = reqwest::blocking::Client::new()
    .post("http://127.0.0.1:8080/api/echo")
    .json(&serde_json::json!({"data": "hello"}))
    .send()?;
println!("{}", res.text()?);  // same bytes back`
    },
    node: {
      loc: "7", lib: "fetch (built-in 18+)",
      code: `const res = await fetch("http://127.0.0.1:8080/api/echo", {
  method: "POST",
  headers: { "Content-Type": "application/json" },
  body: JSON.stringify({ data: "hello" }),
});
console.log((await res.json()).data);  // same bytes back`
    }
  },
  auth: {
    c: {
      loc: "7", lib: "hand-rolled (this server)", real: true,
      code: `/* real code from src/server.c */
if (req->api_key[0] == '\\0') return API_ERR_UNAUTHORIZED;  /* 401 */
return key_matches(server->api_key, req->api_key)  /* constant-time */
    ? API_OK : API_ERR_FORBIDDEN;                  /* 200 / 403 */`
    },
    python: {
      loc: "8", lib: "urllib (stdlib)",
      code: `req = urllib.request.Request(url, data=body, method="PUT",
    headers={"X-API-Key": key})  # header attached by hand
try:
    urllib.request.urlopen(req, timeout=5)
except urllib.error.HTTPError as e:
    print({401: "no/invalid key sent", 403: "wrong key"}[e.code])`
    },
    go: {
      loc: "9", lib: "net/http (stdlib)",
      code: `req.Header.Set("X-API-Key", key)  // header attached by hand
resp, err := http.DefaultClient.Do(req)
switch resp.StatusCode {
case 200: fmt.Println("allowed")
case 401: fmt.Println("no key sent")
case 403: fmt.Println("wrong key")
}`
    },
    rust: {
      loc: "9", lib: "reqwest (crate)",
      code: `let res = client.put(url)
    .header("X-Api-Key", &key)  // header attached by hand
    .body(body).send()?;
match res.status().as_u16() {
    200 => println!("allowed"),
    401 => println!("no key sent"),
    403 => println!("wrong key"),
    s => println!("other: {}", s),
}`
    },
    node: {
      loc: "8", lib: "fetch (built-in 18+)",
      code: `const res = await fetch(url, {  // header attached by hand
  method: "PUT", headers: { "X-API-Key": key }, body,
});
console.log({ 200: "allowed", 401: "no key sent",
              403: "wrong key" }[res.status] || res.status);`
    }
  }
};

/** Descriptive, non-promotional notes per language. */
export const LANGNOTES = {
  en: {
    c: "You manage memory, buffers and syscalls yourself; errors are return codes you must check; threads and mutexes are explicit; SQLite via the libsqlite3 C API.",
    python: "The same op takes far fewer explicit lines (urllib, stdlib); errors surface as exceptions; sqlite3 ships in the stdlib; concurrency via threads or asyncio.",
    go: "Compact stdlib (net/http, encoding/json); errors are values you check; lightweight goroutines for concurrency; SQLite needs an external driver.",
    rust: "Memory safety checked at compile time; reqwest/serde_json/rusqlite are external crates; errors are Result values handled with ?.",
    node: "Built-in fetch (18+), async/await style; failures are throws/rejections; SQLite via the external better-sqlite3 package."
  },
  pt: {
    c: "Você gerencia memória, buffers e syscalls; erros são códigos de retorno; threads e mutex explícitos; SQLite pela C API da libsqlite3.",
    python: "A mesma operação em bem menos linhas explícitas (urllib, stdlib); erros viram exceções; sqlite3 já vem na stdlib; concorrência com threads ou asyncio.",
    go: "Stdlib compacta (net/http, encoding/json); erros são valores; goroutines leves para concorrência; SQLite exige driver externo.",
    rust: "Segurança de memória checada em compilação; reqwest/serde_json/rusqlite são crates externos; erros são Result tratados com ?.",
    node: "Fetch embutido (18+), estilo async/await; falhas são throws/rejections; SQLite via pacote externo better-sqlite3."
  },
  ru: {
    c: "Памятью, буферами и системными вызовами управляете вы; ошибки — коды возврата; потоки и мьютексы явные; SQLite через C API libsqlite3.",
    python: "Та же операция в разы короче (urllib, stdlib); ошибки — исключения; sqlite3 уже в stdlib; потоки или asyncio.",
    go: "Компактная stdlib (net/http, encoding/json); ошибки — значения; лёгкие горутины; для SQLite нужен внешний драйвер.",
    rust: "Безопасность памяти проверяет компилятор; reqwest/serde_json/rusqlite — внешние крейты; ошибки — Result с ?.",
    node: "Встроенный fetch (18+), async/await; ошибки — throws/rejections; SQLite через внешний better-sqlite3."
  }
};

/** Row-per-piece matrix: [i18n key, [C, Python, Go, Rust, Node]]. */
export const PIECES = [
  ["httpL", ["libcurl (external)", "urllib (stdlib)", "net/http (stdlib)", "reqwest (crate)", "fetch (built-in 18+)"]],
  ["jsonL", ["manual parser (this server)", "json (stdlib)", "encoding/json (stdlib)", "serde_json (crate)", "JSON (built-in)"]],
  ["sqliteL", ["libsqlite3 (C API)", "sqlite3 (stdlib)", "database/sql + driver", "rusqlite (crate)", "better-sqlite3 (pkg)"]],
  ["errL", ["return codes, checked by hand", "exceptions", "errors as values", "Result + ?", "throw + rejections"]],
  ["concL", ["pthreads + mutex (explicit)", "threads / asyncio", "goroutines", "threads + Send/Sync", "event loop + async"]],
  ["memL", ["manual (malloc/free)", "GC", "GC", "borrow checker", "GC (V8)"]]
];

export const PIECE_L = {
  en: { httpL: "HTTP client", jsonL: "JSON", sqliteL: "SQLite", errL: "errors", concL: "concurrency", memL: "memory" },
  pt: { httpL: "cliente HTTP", jsonL: "JSON", sqliteL: "SQLite", errL: "erros", concL: "concorrência", memL: "memória" },
  ru: { httpL: "HTTP-клиент", jsonL: "JSON", sqliteL: "SQLite", errL: "ошибки", concL: "параллелизм", memL: "память" }
};
