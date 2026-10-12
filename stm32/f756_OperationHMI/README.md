# F756 Air Monitor

Industrial ambient air monitoring HMI / Ethernet gateway on **NUCLEO-F756ZG**.

This board is **not** the sensor node. Sensing stays on `F401_SensorNode`
(HC-SR04 + MPU6050 + FreeRTOS). This project is the HMI side of the dual-MCU stack.

## Role in the system

```
HC-SR04 + MPU6050  →  F401_SensorNode (realtime FSM, UART CSV)
                              │ USART (later)
                              ▼
                     f756_OperationHMI
                     · LVGL / ILI9488 (when display arrives)
                     · Ethernet telemetry (Modbus TCP / JSON)
                     · ACK gated by NFC badge
                              │ optional
                              ▼
                         ESP32-H2 (BLE)
```

| Project | Board | Job |
|---|---|---|
| `F401_SensorNode` | NUCLEO-F401RE | Sensors, FreeRTOS, FSM, CSV |
| `f756_OperationHMI` | NUCLEO-F756ZG | Air Monitor HMI, Ethernet, operator ACK |
| ESP32-H2 (Arduino/IDF) | DevKit | BLE maintenance channel |

Do **not** move HC-SR04 / MPU6050 onto the F756.

## Current status (bring-up)

CubeMX defaults enabled:

- **USART3** ST-LINK VCP @ `115200 8N1` (PD8 TX / PD9 RX)
- **Ethernet** RMII (LAN8742A) — init present, app not using it yet
- **I2C1** — PN532 NFC on D15/D14 (PB8/PB9)
- **LD1 / LD2 / LD3**, user button

Firmware now:

- Prints a boot banner on USART3
- Blinks **LD1** (green) every 500 ms
- Prints `tick` on the serial port

## Build and run

1. Open this folder in STM32CubeIDE (`workspace_2.2.0`).
2. Select `f756_OperationHMI` → **Build Project**.
3. Plug ST-LINK USB on the F756ZG → **Run**.
4. Serial (Mac):

```bash
ls /dev/cu.usbmodem*
screen /dev/cu.usbmodemXXXX 115200
```

Pass: `F756 AIR MONITOR` boot banner and green LD1 blinking.

## Roadmap (do in order)

1. ~~VCP hello + LED~~ (done)
2. Ethernet link / static-IP status (before display)
3. ILI9488 SPI color bars, then LVGL screen
4. UART link from F401 → parse CSV → show zone / distance / ACK
5. NFC (PN532) session labels
6. Optional: forward same stream to ESP32-H2 BLE

## Docs

- [`ARCHITECTURE.md`](ARCHITECTURE.md) — dual-MCU contracts and pin map

Portfolio/lab project. No SIL / ISO 26262 claim.
