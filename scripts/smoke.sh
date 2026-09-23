#!/usr/bin/env bash
# smoke.sh - Manual end-to-end repro: build, start, echo, concurrent, stop.
set -euo pipefail
PORT="${PORT:-18099}"

make >/dev/null
./build/server --bind 127.0.0.1 --port "$PORT" --threads 4 &
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

echo -n "smoke-payload" | python3 -c "import socket,sys; s=socket.create_connection(('127.0.0.1', int('$PORT')), timeout=3); s.sendall(b'smoke-payload'); data=s.recv(4096); sys.exit(0 if data==b'smoke-payload' else 1)"
echo "smoke echo OK"

python3 -c "
import socket, concurrent.futures
def one(i):
    s = socket.create_connection(('127.0.0.1', $PORT), timeout=3)
    msg = ('c%d' % i).encode()
    s.sendall(msg)
    got = s.recv(4096)
    s.close()
    assert got == msg, (got, msg)
with concurrent.futures.ThreadPoolExecutor(max_workers=8) as ex:
    list(ex.map(one, range(16)))
print('smoke concurrent OK')
"

kill -TERM "$SERVER_PID"
wait "$SERVER_PID"
echo "smoke shutdown OK"
