# Architecture — Air Monitor HMI (F756)

## Dual-MCU split

```
┌─────────────────────────────┐     UART CSV      ┌──────────────────────────────┐
│  F401_SensorNode            │ ───────────────►  │  f756_OperationHMI           │
│  NUCLEO-F401RE              │                   │  NUCLEO-F756ZG               │
│                             │                   │                              │
│  HC-SR04 (PA8/PA9)          │                   │  USART3 VCP (debug)          │
│  MPU6050 (PB8/PB9)          │                   │  ETH RMII (plant / PC)       │
│  FreeRTOS + FSM + IWDG      │                   │  ILI9488 / LVGL (later)      │
│  USART2 → Mac Python        │                   │  I2C1 NFC PN532 (later)      │
└─────────────────────────────┘                   └──────────────┬───────────────┘
                                                                 │ UART (optional)
                                                                 ▼
                                                          ESP32-H2 BLE
```

**Rule:** hard realtime sensing stays on F401. F756 is viewer + ACK + network.

## F756 peripherals (CubeMX defaults)

| Block | Use now | Later |
|---|---|---|
| USART3 (PD8/PD9) | ST-LINK console 115200 | Keep for debug |
| Extra USART | — | Link from F401 TX |
| ETH RMII | Init only | Modbus TCP or JSON `/status` |
| I2C1 (PB8/PB9) | Unused | PN532 NFC |
| LD1 PB0 | Heartbeat (Betrieb) | Alive |
| LD2 PB7 | UART TX | Link activity |
| LD3 PB14 | Fault lamp | Flash = unacked alarm; steady = quittiert; off = healthy |
| USER Btn PC13 | Free | Local ACK |
| SPI | — | ILI9488 + touch CS |

## UART contract (from F401, when linked)

F401 already emits CSV-style rows (see `F401_SensorNode`). F756 will:

1. Receive lines on a dedicated USART
2. Parse `distance`, `imu`, `zone` / state
3. Update LVGL widgets
4. Send ACK (**Quittierung**, IEC 62682 / VDI 3699): silence + log operator UID. Do not clear the alarm until the process is healthy.

Until that link exists, use USART3 only for bring-up text.

## Alarm ACK (Quittierung)

Unacknowledged alarm: LD3 flashes, banner pulses. Operator ACK (NFC session if badge is online) silences the lamp/flash and writes `ACK_OK t=.. uid=.. pri=..`. The alarm text stays until temperature/humidity/DHT is back in range. A new alarm identity requires a new ACK.

## Build stages

1. Bring-up (current): LED + VCP
2. Ethernet alive (static IP ping / hello)
3. Display SPI + LVGL (UI drafted in `ui/` for LVGL Pro simulator)
4. F401 UART ingest → bind `temp_c` / `hum_pct` / `distance_cm` subjects
5. NFC + BLE gateway

## Out of scope on F756

- HC-SR04 input-capture stack
- MPU6050 100 Hz driver as primary node
- Claiming AUTOSAR / ISO 26262
