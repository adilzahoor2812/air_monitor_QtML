# Air Monitor — LVGL Pro UI

Open this folder in **LVGL Pro Editor** (File → Open Project → select `ui/`).

## Targets
| Target | Size | Use |
|--------|------|-----|
| `ili9488_480x320` | 480×320 | Default SPI TFT |
| `panel_800x480` | 800×480 | Larger panel |

## Screen
`screens/screen_dashboard.xml` — permanent main operator view.

## Data subjects (edit live in simulator)
| Subject | Type | Meaning |
|---------|------|---------|
| `temp_c` | float | DHT22 temperature °C |
| `hum_pct` | float | DHT22 humidity % |
| `distance_cm` | int | F401 HC-SR04 cm |
| `zone` | int | 0 SAFE / 1 WARN / 2 FAULT |
| `zone_label` | string | Shown zone text |
| `status_line` | string | Footer status |
| `link_f401` | int | 1 = F401 UART up |
| `link_eth` | int | 1 = Ethernet up |
| `link_nfc` | int | 1 = PN532 online |
| `ack_latched` | int | Set by ACK button |

## NFC (PN532 I2C)

Wiring: **SCL→D15**, **SDA→D14**, VCC→3V3, GND→GND (I2C DIP mode).

Serial lines the simulator understands:

| Line | Meaning |
|------|---------|
| `TEMP=.. HUM=..` | Live DHT22 |
| `CPU=.. HEAP=..` | F756 busy % and heap free % |
| `NFC=OK` / `NFC=FAIL` | Module bring-up |
| `NFC_UID=AA:BB:…` | Badge tapped |
| `NFC_ARMED` / `NFC_LOCKED` | ACK unlock window |
| `ACK_OK t=.. uid=.. pri=ALARM` | Quittierung accepted (IEC 62682) |
| `ACK_DENIED` | ACK rejected — badge required |

Tap badge → ACK unlocks ~30 s → press ACK → alarm is **acknowledged** (silenced, logged, not cleared). LD3 flashes while unacked, then stays ON until the process is healthy.

# LVGL UI (real — deploy to display)

This is the **actual LVGL C UI** (not a browser page).

| Path | Role |
|------|------|
| `ui/lvgl/operation_hmi.c/.h` | Portable HMI screen (480×320) |
| `ui/simulator/` | Mac SDL window using those same sources + live F756 serial |
| `ui/*.xml` | Optional LVGL Pro design source |

## Run on Mac (proper LVGL window)

Quit Terminal `screen`/`cat` on the Nucleo port first.

```bash
cd /Users/adilzahoormalik/STM32CubeIDE/workspace_2.2.0/f756_OperationHMI/ui/simulator
git clone --depth 1 --branch v9.2.2 https://github.com/lvgl/lvgl.git third_party/lvgl
cp lv_conf.h third_party/lvgl/lv_conf.h
cmake -S . -B build
cmake --build build -j
./build/operation_hmi_sim
```

Or: `./build/operation_hmi_sim /dev/cu.usbmodem1103`

Needs: CMake, SDL2 (`brew install sdl2`).

## Deploy on F756 + display

1. Add LVGL 9 to CubeIDE.
2. Bring up display (SPI ILI9488 480×320).
3. Copy `ui/lvgl/operation_hmi.c` + `.h` into firmware.
4. Call `operation_hmi_create()` then update widgets from DHT22 / F401.

See `ui/lvgl/README.md`.

## Live sensor preview (Mac, no display needed)

**LVGL Pro preview cannot stream Nucleo UART.** Use this live simulator instead (same HMI layout + real TEMP/HUM):

1. Quit Terminal `screen` / `cat` on the serial port (`Ctrl+A` then `K` in screen).
2. Run:

```bash
cd /Users/adilzahoormalik/STM32CubeIDE/workspace_2.2.0/f756_OperationHMI/ui/Python
pip3 install -r requirements.txt
python3 live_hmi.py
```

Browser opens at http://127.0.0.1:8765 with the attractive panel UI and live DHT22 values.

Optional: `python3 live_hmi.py --port /dev/cu.usbmodem1103`

## Later on F756
1. Export C from Pro Editor  
2. Add LVGL + display driver  
3. Call `operation_hmi_init(...)` then load `screen_dashboard`  
4. Update subjects from sensors, e.g. `lv_subject_set_float(&temp_c, t)`

Widget names for C: `lbl_temp`, `lbl_hum`, `lbl_dist`, `lbl_zone`, `btn_ack`.
