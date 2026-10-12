#!/usr/bin/env python3
"""Live Operation HMI simulator: F756 USART3 → attractive on-screen panel.

LVGL Pro Editor cannot stream Nucleo UART into its preview.
This is the live simulator for real sensor data until a physical display exists.

  # IMPORTANT: close screen/cat on the serial port first (only one app can use it)
  cd ui/Python
  pip3 install -r requirements.txt
  python3 live_hmi.py

Opens http://127.0.0.1:8765 automatically.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
import threading
import time
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    sys.stderr.write("Install deps: pip3 install -r requirements.txt\n")
    sys.exit(1)

TEMP_HUM_RE = re.compile(
    r"TEMP=(-?\d+)\.(\d)\s*C\s+HUM=(\d+)\.(\d)\s*%",
    re.IGNORECASE,
)
FAIL_RE = re.compile(r"DHT22 fail err=(-?\d+)", re.IGNORECASE)

STATE = {
    "temp_c": None,
    "hum_pct": None,
    "distance_cm": None,
    "zone_label": "—",
    "status_line": "waiting for F756 serial…",
    "link_f401": 0,
    "link_eth": 0,
    "ok": False,
    "port": "",
    "updated_ms": 0,
    "raw": "",
}
STATE_LOCK = threading.Lock()

HTML = r"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<title>Operation HMI · Live Simulator</title>
<link rel="preconnect" href="https://fonts.googleapis.com"/>
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin/>
<link href="https://fonts.googleapis.com/css2?family=IBM+Plex+Sans:wght@400;500;600;700&family=IBM+Plex+Mono:wght@500&display=swap" rel="stylesheet"/>
<style>
  :root {
    --bg: #070b14;
    --desk: #0e1524;
    --bezel: #1a2233;
    --panel: #151e2e;
    --panel-hi: #1c273a;
    --line: #2a364a;
    --text: #e8eef7;
    --muted: #8b9bb4;
    --accent: #2ec4b6;
    --warn: #f4a261;
    --ok: #3ddc97;
    --fault: #e63946;
    --dist: #5b8def;
  }
  * { box-sizing: border-box; }
  body {
    margin: 0; min-height: 100vh;
    font-family: "IBM Plex Sans", system-ui, sans-serif;
    color: var(--text);
    background:
      radial-gradient(1200px 600px at 20% -10%, #143047 0%, transparent 55%),
      radial-gradient(900px 500px at 90% 110%, #1a2a22 0%, transparent 50%),
      var(--bg);
    display: grid;
    place-items: center;
    padding: 28px 16px 40px;
  }
  .stage { width: min(920px, 100%); }
  .eyebrow {
    display: flex; justify-content: space-between; align-items: baseline;
    margin-bottom: 14px; color: var(--muted); font-size: 13px;
  }
  .eyebrow strong { color: var(--accent); font-weight: 600; letter-spacing: 0.08em; }
  .device {
    background: linear-gradient(160deg, #2a3348, #121826 55%, #0c111c);
    border-radius: 22px;
    padding: 18px 18px 22px;
    box-shadow:
      0 30px 80px rgba(0,0,0,0.55),
      inset 0 1px 0 rgba(255,255,255,0.08);
  }
  .bezel {
    background: #05080f;
    border-radius: 10px;
    padding: 8px;
    border: 1px solid #2a3142;
  }
  /* 480×320 panel aspect */
  .panel {
    width: 100%;
    aspect-ratio: 480 / 320;
    background: var(--desk);
    border-radius: 4px;
    overflow: hidden;
    display: flex;
    flex-direction: column;
    position: relative;
  }
  .panel::after {
    content: "";
    pointer-events: none;
    position: absolute; inset: 0;
    background: linear-gradient(180deg, rgba(255,255,255,0.04), transparent 28%);
  }
  header {
    height: 15%;
    min-height: 44px;
    display: flex; align-items: center; justify-content: space-between;
    padding: 0 3.2%;
    background: var(--panel-hi);
    border-bottom: 1px solid var(--line);
  }
  header h1 {
    margin: 0; font-size: clamp(11px, 2.2vw, 14px);
    letter-spacing: 0.2em; font-weight: 700;
  }
  header p {
    margin: 2px 0 0; color: var(--muted);
    font-size: clamp(9px, 1.6vw, 11px);
  }
  .pills { display: flex; gap: 8px; }
  .pill {
    display: flex; align-items: center; gap: 6px;
    padding: 4px 10px; border-radius: 999px;
    background: var(--panel); border: 1px solid var(--line);
    color: var(--muted); font-size: clamp(9px, 1.5vw, 11px);
  }
  .dot {
    width: 7px; height: 7px; border-radius: 50%;
    background: var(--muted);
    transition: background 0.25s, box-shadow 0.25s;
  }
  .dot.on {
    background: var(--ok);
    box-shadow: 0 0 8px rgba(61, 220, 151, 0.7);
  }
  .body {
    flex: 1;
    padding: 2.4% 2.6%;
    display: flex; flex-direction: column; gap: 2.4%;
    min-height: 0;
  }
  .metrics {
    flex: 1.2;
    display: grid;
    grid-template-columns: repeat(3, 1fr);
    gap: 2.4%;
    min-height: 0;
  }
  .card {
    background: var(--panel);
    border: 1px solid var(--line);
    border-radius: 10px;
    padding: 3.5% 4%;
    display: flex; flex-direction: column;
    min-width: 0;
  }
  .bar { height: 4px; border-radius: 2px; margin-bottom: 8px; width: 100%; }
  .bar.temp { background: var(--warn); }
  .bar.hum { background: var(--accent); }
  .bar.dist { background: var(--dist); }
  .label {
    color: var(--muted);
    font-size: clamp(8px, 1.5vw, 11px);
    letter-spacing: 0.14em;
    font-weight: 600;
  }
  .value-row {
    margin-top: auto;
    display: flex; align-items: baseline; gap: 4px;
    font-variant-numeric: tabular-nums;
  }
  .value {
    font-family: "IBM Plex Mono", ui-monospace, monospace;
    font-size: clamp(22px, 5.2vw, 40px);
    font-weight: 600;
    line-height: 1;
    transition: color 0.2s;
  }
  .unit {
    font-size: clamp(11px, 2vw, 15px);
    color: var(--muted);
    font-weight: 500;
  }
  .unit.temp { color: var(--warn); }
  .unit.hum { color: var(--accent); }
  .unit.dist { color: var(--dist); }
  .bottom {
    flex: 0.85;
    display: grid;
    grid-template-columns: 1.7fr 1fr;
    gap: 2.4%;
    min-height: 0;
  }
  .zone-val {
    margin: 6px 0 4px;
    font-size: clamp(18px, 3.6vw, 28px);
    font-weight: 700;
    color: var(--ok);
    letter-spacing: 0.06em;
  }
  .zone-val.warn { color: var(--warn); }
  .zone-val.fault { color: var(--fault); }
  .status {
    color: var(--muted);
    font-size: clamp(10px, 1.7vw, 12px);
    line-height: 1.35;
  }
  .status.err { color: var(--fault); }
  .ack-wrap {
    display: flex; flex-direction: column;
    align-items: center; justify-content: center; gap: 8px;
  }
  .ack {
    border: 0; cursor: default;
    background: var(--accent); color: #071018;
    font-weight: 700; letter-spacing: 0.18em;
    border-radius: 8px;
    padding: 12px 28px;
    font-size: clamp(12px, 2vw, 14px);
    box-shadow: 0 8px 24px rgba(46, 196, 182, 0.25);
  }
  .live-tag {
    display: inline-flex; align-items: center; gap: 6px;
    font-size: 11px; color: var(--muted);
  }
  .pulse {
    width: 6px; height: 6px; border-radius: 50%;
    background: var(--fault);
    animation: pulse 1.2s infinite;
  }
  .pulse.on { background: var(--ok); }
  @keyframes pulse {
    0%, 100% { opacity: 1; transform: scale(1); }
    50% { opacity: 0.35; transform: scale(0.85); }
  }
  .foot {
    margin-top: 14px;
    display: flex; justify-content: space-between; gap: 12px;
    color: var(--muted); font-size: 12px;
    font-family: "IBM Plex Mono", monospace;
  }
  .hint { margin-top: 10px; color: #6d7c94; font-size: 12px; line-height: 1.45; }
</style>
</head>
<body>
  <div class="stage">
    <div class="eyebrow">
      <span><strong>LIVE SIMULATOR</strong> · real F756 DHT22</span>
      <span class="live-tag"><span class="pulse" id="pulse"></span><span id="live-text">connecting</span></span>
    </div>

    <div class="device">
      <div class="bezel">
        <div class="panel">
          <header>
            <div>
              <h1>OPERATION HMI</h1>
              <p>F756 · sensor gateway</p>
            </div>
            <div class="pills">
              <div class="pill"><span class="dot" id="dot-f756"></span>F756</div>
              <div class="pill"><span class="dot" id="dot-f401"></span>F401</div>
              <div class="pill"><span class="dot" id="dot-eth"></span>ETH</div>
            </div>
          </header>

          <div class="body">
            <div class="metrics">
              <div class="card">
                <div class="bar temp"></div>
                <div class="label">TEMPERATURE</div>
                <div class="value-row">
                  <span class="value" id="temp">--.-</span>
                  <span class="unit temp">°C</span>
                </div>
              </div>
              <div class="card">
                <div class="bar hum"></div>
                <div class="label">HUMIDITY</div>
                <div class="value-row">
                  <span class="value" id="hum">--.-</span>
                  <span class="unit hum">%</span>
                </div>
              </div>
              <div class="card">
                <div class="bar dist"></div>
                <div class="label">DISTANCE</div>
                <div class="value-row">
                  <span class="value" id="dist">—</span>
                  <span class="unit dist">cm</span>
                </div>
              </div>
            </div>

            <div class="bottom">
              <div class="card">
                <div class="label">PROXIMITY ZONE</div>
                <div class="zone-val" id="zone">—</div>
                <div class="status" id="status">waiting for serial…</div>
              </div>
              <div class="card ack-wrap">
                <div class="label">OPERATOR</div>
                <button class="ack" type="button">ACK</button>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>

    <div class="foot">
      <span id="meta">—</span>
      <span id="raw">—</span>
    </div>
    <p class="hint">
      Same layout as the LVGL Pro UI. Close any <code>screen</code>/<code>cat</code> on the serial port first,
      then run <code>python3 live_hmi.py</code> so this simulator can read the Nucleo.
    </p>
  </div>

<script>
function setDot(id, on) {
  document.getElementById(id).className = 'dot' + (on ? ' on' : '');
}
async function tick() {
  try {
    const r = await fetch('/api/state');
    const s = await r.json();
    document.getElementById('temp').textContent =
      s.temp_c == null ? '--.-' : Number(s.temp_c).toFixed(1);
    document.getElementById('hum').textContent =
      s.hum_pct == null ? '--.-' : Number(s.hum_pct).toFixed(1);
    document.getElementById('dist').textContent =
      s.distance_cm == null ? '—' : String(s.distance_cm);

    const zone = document.getElementById('zone');
    zone.textContent = s.zone_label || '—';
    zone.className = 'zone-val' +
      (s.zone_label === 'WARN' ? ' warn' : s.zone_label === 'FAULT' ? ' fault' : '');

    const st = document.getElementById('status');
    st.textContent = s.status_line || '';
    st.className = 'status' + (s.ok ? '' : ' err');

    setDot('dot-f756', !!s.ok);
    setDot('dot-f401', !!s.link_f401);
    setDot('dot-eth', !!s.link_eth);
    document.getElementById('pulse').className = 'pulse' + (s.ok ? ' on' : '');
    document.getElementById('live-text').textContent = s.ok ? 'live data' : 'no sample yet';
    document.getElementById('meta').textContent = s.port
      ? (s.port + ' @ 115200') : 'no port';
    document.getElementById('raw').textContent = s.raw || '';
  } catch (e) {
    document.getElementById('status').textContent = 'simulator server not reachable';
    document.getElementById('status').className = 'status err';
  }
}
setInterval(tick, 400);
tick();
</script>
</body>
</html>
"""


def default_port() -> str | None:
    ports = [p.device for p in list_ports.comports() if "usbmodem" in p.device.lower()]
    if not ports:
        ports = sorted(str(p) for p in Path("/dev").glob("cu.usbmodem*"))
    if ports:
        return ports[0]
    return None


def set_state(**kwargs) -> None:
    with STATE_LOCK:
        STATE.update(kwargs)
        STATE["updated_ms"] = int(time.time() * 1000)


def serial_worker(port: str, baud: int) -> None:
    set_state(port=port, status_line=f"opening {port}…", ok=False)
    while True:
        try:
            with serial.Serial(port, baudrate=baud, timeout=1) as ser:
                set_state(status_line=f"listening on {port}", ok=False, raw="")
                print(f"[simulator] connected {port} @ {baud}", flush=True)
                while True:
                    raw = ser.readline()
                    if not raw:
                        continue
                    line = raw.decode("utf-8", errors="replace").strip()
                    if not line:
                        continue
                    print(line, flush=True)
                    set_state(raw=line)

                    m = TEMP_HUM_RE.search(line)
                    if m:
                        temp = float(f"{m.group(1)}.{m.group(2)}")
                        hum = float(f"{m.group(3)}.{m.group(4)}")
                        set_state(
                            temp_c=temp,
                            hum_pct=hum,
                            zone_label="SAFE",
                            status_line="DHT22 OK · live from F756",
                            ok=True,
                        )
                        continue

                    fail = FAIL_RE.search(line)
                    if fail:
                        set_state(
                            ok=False,
                            status_line=f"DHT22 fail err={fail.group(1)} — check OUT→D2",
                        )
        except serial.SerialException as exc:
            set_state(ok=False, status_line=f"serial busy/error: {exc}")
            print(f"[simulator] serial error: {exc}", flush=True)
            print(
                "[simulator] Quit Terminal screen/cat (Ctrl+A K), then restart this script.",
                flush=True,
            )
            time.sleep(2)


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
            with STATE_LOCK:
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
    ap = argparse.ArgumentParser(description="F756 serial → live HMI simulator UI")
    ap.add_argument("--port", help="e.g. /dev/cu.usbmodem1103")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--http", type=int, default=8765)
    ap.add_argument("--no-browser", action="store_true")
    args = ap.parse_args()

    port = args.port or default_port()
    if not port:
        sys.stderr.write(
            "No /dev/cu.usbmodem* found. Plug Nucleo F756, then:\n"
            "  ls /dev/cu.usbmodem*\n"
            "  python3 live_hmi.py --port /dev/cu.usbmodemXXXX\n"
        )
        return 1

    threading.Thread(target=serial_worker, args=(port, args.baud), daemon=True).start()

    url = f"http://127.0.0.1:{args.http}"
    server = ThreadingHTTPServer(("127.0.0.1", args.http), Handler)
    print(f"[simulator] UI → {url}", flush=True)
    print(f"[simulator] serial {port} @ {args.baud}", flush=True)
    print("[simulator] Close any Terminal 'screen' on this port first.", flush=True)

    if not args.no_browser:
        threading.Timer(0.6, lambda: webbrowser.open(url)).start()

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n[simulator] stopped", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
