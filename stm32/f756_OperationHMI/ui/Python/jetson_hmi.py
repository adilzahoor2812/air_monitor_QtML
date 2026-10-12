#!/usr/bin/env python3
"""Jetson / host AIR MONITOR over Ethernet.

Connects to F756 TCP telemetry at 192.168.10.2:5000 and serves a live UI.

  # On Jetson (eth0 = 192.168.10.10/24, cable to Nucleo RJ45):
  cd ui/Python
  pip3 install -r requirements.txt
  python3 jetson_hmi.py

Opens http://127.0.0.1:8766
"""

from __future__ import annotations

import argparse
import json
import socket
import threading
import time
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

HOST_DEFAULT = "192.168.10.2"
PORT_DEFAULT = 5000

STATE = {
    "temp_c": None,
    "hum_pct": None,
    "distance_cm": None,
    "cpu": None,
    "heap": None,
    "zone": "—",
    "ok": False,
    "online": False,
    "freertos": None,
    "watchdog": None,
    "link_eth": 0,
    "link_nfc": 0,
    "link_f401": 0,
    "status_line": "LIVE offline — set Jetson Ethernet to 192.168.10.10/24, then reach 192.168.10.2:5000",
    "raw": "",
    "updated_ms": 0,
}
LOCK = threading.Lock()

HTML = r"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<title>AIR MONITOR · LIVE</title>
<style>
  :root {
    --bg:#0b1220; --panel:#151e2e; --line:#2a364a; --text:#e8eef7;
    --muted:#8b9bb4; --ok:#3ddc97; --fault:#e63946; --warn:#f4a261; --accent:#5b8def;
  }
  *{box-sizing:border-box}
  body{margin:0;min-height:100vh;font-family:system-ui,sans-serif;background:var(--bg);color:var(--text);
    display:grid;place-items:center;padding:24px}
  .win{width:min(980px,100%);background:linear-gradient(160deg,#1b2436,#101826);border:1px solid var(--line);
    border-radius:10px;overflow:hidden;box-shadow:0 20px 60px rgba(0,0,0,.45)}
  header{display:flex;justify-content:space-between;align-items:center;padding:14px 18px;border-bottom:1px solid var(--line)}
  h1{margin:0;font-size:18px;letter-spacing:.12em}
  .badge{display:inline-flex;align-items:center;gap:8px;font-weight:700}
  .dot{width:10px;height:10px;border-radius:50%;background:var(--fault)}
  .dot.on{background:var(--ok)}
  .banner{padding:10px 18px;background:#3a1520;color:#ffb4b4;font-size:13px;border-bottom:1px solid #5a2230}
  .banner.hide{display:none}
  .grid{display:grid;grid-template-columns:1fr 1.2fr 1fr;gap:14px;padding:16px}
  .col{background:var(--panel);border:1px solid var(--line);border-radius:8px;padding:14px;min-height:280px}
  .col h2{margin:0 0 12px;font-size:12px;letter-spacing:.14em;color:var(--muted)}
  .row{display:flex;justify-content:space-between;padding:8px 0;border-bottom:1px solid #243044;font-size:14px}
  .row:last-child{border-bottom:0}
  .k{color:var(--muted)} .v{font-variant-numeric:tabular-nums}
  .v.bad{color:var(--fault)} .v.good{color:var(--ok)}
  .metric{margin:10px 0 18px}
  .metric .name{font-size:12px;color:var(--muted);letter-spacing:.08em}
  .metric .val{font-size:34px;font-weight:700;margin-top:4px}
  .unit{font-size:14px;color:var(--muted);font-weight:500}
</style>
</head>
<body>
  <div class="win">
    <header>
      <h1>AIR MONITOR</h1>
      <div class="badge"><span class="dot" id="dot"></span><span id="online">OFFLINE</span></div>
    </header>
    <div class="banner" id="banner">LIVE offline — set Jetson Ethernet to 192.168.10.10/24, then reach 192.168.10.2:5000</div>
    <div class="grid">
      <div class="col">
        <h2>PLANT</h2>
        <div class="row"><span class="k">Fault</span><span class="v" id="fault">—</span></div>
        <div class="row"><span class="k">Zone</span><span class="v" id="zone">—</span></div>
        <div class="row"><span class="k">NFC</span><span class="v" id="nfc">—</span></div>
        <div class="row"><span class="k">F401</span><span class="v" id="f401">—</span></div>
      </div>
      <div class="col">
        <h2>PROCESS</h2>
        <div class="metric"><div class="name">DISTANCE</div><div class="val"><span id="dist">—</span> <span class="unit">cm</span></div></div>
        <div class="metric"><div class="name">TEMPERATURE</div><div class="val"><span id="temp">—</span> <span class="unit">°C</span></div></div>
        <div class="metric"><div class="name">HUMIDITY</div><div class="val"><span id="hum">—</span> <span class="unit">%</span></div></div>
      </div>
      <div class="col">
        <h2>SYSTEM</h2>
        <div class="row"><span class="k">Ethernet</span><span class="v" id="eth">—</span></div>
        <div class="row"><span class="k">FreeRTOS</span><span class="v" id="rtos">—</span></div>
        <div class="row"><span class="k">Watchdog</span><span class="v" id="wdg">—</span></div>
        <div class="row"><span class="k">CPU</span><span class="v" id="cpu">—</span></div>
        <div class="row"><span class="k">Heap</span><span class="v" id="heap">—</span></div>
      </div>
    </div>
  </div>
<script>
async function tick(){
  try{
    const r = await fetch('/api/state');
    const s = await r.json();
    const on = !!s.online;
    document.getElementById('dot').className = 'dot' + (on ? ' on' : '');
    document.getElementById('online').textContent = on ? 'ONLINE' : 'OFFLINE';
    document.getElementById('banner').className = 'banner' + (on ? ' hide' : '');
    document.getElementById('banner').textContent = s.status_line || '';
    document.getElementById('temp').textContent = s.temp_c == null ? '—' : Number(s.temp_c).toFixed(1);
    document.getElementById('hum').textContent = s.hum_pct == null ? '—' : Number(s.hum_pct).toFixed(1);
    document.getElementById('dist').textContent = (s.distance_cm == null || s.distance_cm < 0) ? '—' : s.distance_cm;
    document.getElementById('zone').textContent = s.zone || '—';
    document.getElementById('fault').textContent = s.ok ? 'keine' : 'DHT22';
    document.getElementById('nfc').textContent = s.link_nfc ? 'OK' : '—';
    document.getElementById('f401').textContent = s.link_f401 ? 'OK' : '—';
    const eth = document.getElementById('eth');
    eth.textContent = on ? 'Connected' : 'Offline';
    eth.className = 'v ' + (on ? 'good' : 'bad');
    document.getElementById('rtos').textContent = s.freertos ? 'OK' : '—';
    document.getElementById('wdg').textContent = s.watchdog ? 'OK' : '—';
    document.getElementById('cpu').textContent = s.cpu == null ? '—' : (s.cpu + ' %');
    document.getElementById('heap').textContent = s.heap == null ? '—' : (s.heap + ' % free');
  }catch(e){}
}
setInterval(tick, 400); tick();
</script>
</body>
</html>
"""


def set_state(**kwargs) -> None:
    with LOCK:
        STATE.update(kwargs)
        STATE["updated_ms"] = int(time.time() * 1000)


def tcp_worker(host: str, port: int) -> None:
    buf = ""
    while True:
        try:
            set_state(
                online=False,
                status_line=(
                    f"LIVE offline — set Jetson Ethernet to 192.168.10.10/24, "
                    f"then reach {host}:{port}"
                ),
            )
            with socket.create_connection((host, port), timeout=3) as sock:
                sock.settimeout(2.0)
                set_state(online=True, status_line=f"LIVE · {host}:{port}", link_eth=1)
                print(f"[jetson_hmi] connected {host}:{port}", flush=True)
                while True:
                    chunk = sock.recv(512)
                    if not chunk:
                        break
                    buf += chunk.decode("utf-8", errors="replace")
                    while "\n" in buf:
                        line, buf = buf.split("\n", 1)
                        line = line.strip()
                        if not line:
                            continue
                        print(line, flush=True)
                        try:
                            msg = json.loads(line)
                        except json.JSONDecodeError:
                            continue
                        set_state(
                            online=True,
                            ok=bool(msg.get("ok")),
                            temp_c=msg.get("temp_c"),
                            hum_pct=msg.get("hum_pct"),
                            distance_cm=msg.get("distance_cm"),
                            cpu=msg.get("cpu"),
                            heap=msg.get("heap"),
                            zone=msg.get("zone", "—"),
                            freertos=msg.get("freertos"),
                            watchdog=msg.get("watchdog"),
                            link_eth=msg.get("link_eth", 1),
                            link_nfc=msg.get("link_nfc", 0),
                            link_f401=msg.get("link_f401", 0),
                            raw=line,
                            status_line="LIVE · telemetry OK",
                        )
        except OSError as exc:
            print(f"[jetson_hmi] reconnect: {exc}", flush=True)
            set_state(online=False)
            time.sleep(1.5)


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt: str, *args) -> None:
        return

    def do_GET(self) -> None:
        if self.path in ("/", "/index.html"):
            body = HTML.encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        if self.path == "/api/state":
            with LOCK:
                payload = json.dumps(STATE).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Content-Length", str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)
            return
        self.send_error(404)


def main() -> int:
    ap = argparse.ArgumentParser(description="F756 Ethernet → AIR MONITOR UI")
    ap.add_argument("--host", default=HOST_DEFAULT)
    ap.add_argument("--port", type=int, default=PORT_DEFAULT)
    ap.add_argument("--http", type=int, default=8766)
    ap.add_argument("--no-browser", action="store_true")
    args = ap.parse_args()

    threading.Thread(target=tcp_worker, args=(args.host, args.port), daemon=True).start()
    url = f"http://127.0.0.1:{args.http}"
    server = ThreadingHTTPServer(("0.0.0.0", args.http), Handler)
    print(f"[jetson_hmi] UI → {url}", flush=True)
    print(f"[jetson_hmi] TCP → {args.host}:{args.port}", flush=True)
    if not args.no_browser:
        threading.Timer(0.6, lambda: webbrowser.open(url)).start()
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n[jetson_hmi] stopped", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
