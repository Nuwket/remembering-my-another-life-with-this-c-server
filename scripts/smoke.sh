#!/usr/bin/env bash
# smoke.sh - Manual end-to-end repro: build, start, health/kv/notes, concurrent, stop.
set -euo pipefail
PORT="${PORT:-18099}"
DB="${DB:-./data/smoke.db}"

make >/dev/null
rm -f "$DB" "$DB-wal" "$DB-shm"
./build/server --bind 127.0.0.1 --port "$PORT" --threads 4 --db "$DB" &
SERVER_PID=$!
trap 'kill -TERM $SERVER_PID 2>/dev/null || true; wait $SERVER_PID 2>/dev/null || true' EXIT

for i in $(seq 1 50); do
  if (echo > /dev/tcp/127.0.0.1/"$PORT") 2>/dev/null; then
    break
  fi
  sleep 0.1
  if [ "$i" -eq 50 ]; then
    echo "server not ready" >&2
    exit 1
  fi
done

python3 - "$PORT" <<'EOF'
import json, sys, urllib.request
port = sys.argv[1]
base = "http://127.0.0.1:%s" % port

def call(method, path, payload=None):
    data = json.dumps(payload).encode() if payload is not None else None
    req = urllib.request.Request(base + path, data=data, method=method,
                                 headers={"Content-Type": "application/json"} if data else {})
    try:
        with urllib.request.urlopen(req, timeout=5) as r:
            return r.status, r.read().decode()
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode()

s, b = call("GET", "/health")
assert s == 200 and '"ok"' in b, (s, b)
print("smoke health OK")

s, b = call("PUT", "/api/kv/smoke", {"value": "dark"})
assert s == 200 and "dark" in b, (s, b)
s, b = call("GET", "/api/kv/smoke")
assert s == 200 and "dark" in b, (s, b)
s, b = call("GET", "/api/kv/missing")
assert s == 404, (s, b)
print("smoke kv OK")

s, b = call("POST", "/api/notes", {"title": "t1", "body": "b1"})
assert s == 201 and '"t1"' in b, (s, b)
nid = json.loads(b)["id"]
s, b = call("GET", "/api/notes/%d" % nid)
assert s == 200, (s, b)
s, b = call("GET", "/api/notes?limit=10")
assert s == 200 and '"total":1' in b, (s, b)
print("smoke notes OK")

s, b = call("GET", "/nope")
assert s == 404, (s, b)
s, b = call("POST", "/health", {})
assert s == 405, (s, b)
print("smoke errors OK")
EOF

python3 - "$PORT" <<'EOF'
import concurrent.futures, json, sys, urllib.request
port = sys.argv[1]
def one(i):
    data = json.dumps({"value": "v"}).encode()
    req = urllib.request.Request("http://127.0.0.1:%s/api/kv/c%d" % (port, i), data=data, method="PUT",
                                 headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=5) as r:
        assert r.status == 200, r.status
with concurrent.futures.ThreadPoolExecutor(max_workers=8) as ex:
    list(ex.map(one, range(16)))
print("smoke concurrent OK")
EOF

sqlite3 "$DB" "SELECT COUNT(*) FROM kv;" 2>/dev/null || python3 -c "import sqlite3; print(sqlite3.connect('$DB').execute('SELECT COUNT(*) FROM kv').fetchone()[0])"

kill -TERM "$SERVER_PID"
wait "$SERVER_PID"
echo "smoke shutdown OK"
