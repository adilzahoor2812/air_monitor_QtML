/**
 * @file app_net.h
 * @brief Minimal Ethernet stack for Nucleo-F756ZG ↔ Jetson link.
 *
 * Static IP 192.168.10.2/24, answers ARP + ICMP, TCP server on port 5000.
 * Streams newline JSON telemetry once a client connects.
 */
#ifndef APP_NET_H
#define APP_NET_H

#include <stdint.h>
#include "stm32f7xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_NET_TCP_PORT 5000U

typedef struct
{
  float temp_c;
  float hum_pct;
  int32_t distance_cm; /* <0 unknown */
  int16_t cpu_pct;
  int16_t heap_pct;
  uint8_t ok;          /* DHT22 valid */
  uint8_t link_nfc;
  uint8_t link_f401;
  char zone[12];       /* SAFE / WARN / FAULT */
} AppNetTelemetry_t;

int AppNet_Init(ETH_HandleTypeDef *heth, ETH_TxPacketConfig *tx_cfg);
void AppNet_Poll(void);
uint8_t AppNet_LinkUp(void);
uint8_t AppNet_TcpConnected(void);

/** Publish latest sensor/system values for the TCP stream. */
void AppNet_SetTelemetry(const AppNetTelemetry_t *t);

/** Optional log sink (USART3). May be NULL. */
void AppNet_SetLog(void (*fn)(const char *msg));

#ifdef __cplusplus
}
#endif

#endif /* APP_NET_H */
