# AIR MONITOR

Fixed 480×320 Qt 6 operator panel for Jetson ↔ STM32F756ZG Ethernet.

## Live mode (default — real STM32 data)

```bash
export PATH="$HOME/Qt/Tools/CMake/bin:$HOME/Qt/Tools/Ninja:$PATH"
export CMAKE_PREFIX_PATH="$HOME/Qt/6.11.2/gcc_arm64"

cmake -S "$HOME/SensorDashboard" -B "$HOME/SensorDashboard/build" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$HOME/SensorDashboard/build"

# 1) Put Jetson Ethernet on the same subnet as the STM32 (needs your password once):
bash "$HOME/SensorDashboard/scripts/setup-jetson-eth.sh"

# 2) Run LIVE UI (no demo values):
"$HOME/SensorDashboard/build/air_monitor" --host 192.168.10.2 --port 5000
```

Demo only if you need UI without the board:

```bash
"$HOME/SensorDashboard/build/air_monitor" --demo
```

## Network

| Device | Example IP |
|--------|------------|
| Jetson `enP8p1s0` | `192.168.10.10/24` |
| STM32F756ZG | `192.168.10.2` |
| TCP port | `5000` |

Cable: STM32 RJ45 ↔ Jetson Ethernet. Link light must be on. STM32 must use a **static** IP (not DHCP-only).

## STM32 → Jetson telemetry (TCP port 5000, one JSON object per LF line)

```text
{"ok":1,"temp_c":22.0,"hum_pct":55.8,"distance_cm":-1,"cpu":1,"heap":99,"link_eth":1,"link_nfc":0,"link_f401":0,"freertos":1,"watchdog":1,"zone":"SAFE"}
```

CSV is also accepted as a fallback:

```text
temp_c,hum_pct,distance_cm,zone,cpu_pct,heap_free_pct,nfc,ack_armed
24.1,48.0,72,SAFE,18,62,0,0
```

`distance_cm: -1` means unknown (shown as —).  
`zone` = `SAFE` | `WARN` | `FAULT`.  
Jetson acknowledge writes `ACK\n` to the socket.
