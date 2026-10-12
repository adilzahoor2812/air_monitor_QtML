/**
 * @file app_net.c
 * @brief Bare-metal ARP + ICMP + tiny TCP server (no LwIP).
 *
 * Jetson: 192.168.10.10/24  →  STM32: 192.168.10.2:5000
 */
#include "app_net.h"

#include <string.h>
#include <stdio.h>

extern ETH_HandleTypeDef heth;

#define APP_NET_RX_BUF_SIZE 1524U
#define APP_NET_RX_POOL     6U
#define APP_NET_TX_BUF_SIZE 1524U

#define ETH_TYPE_ARP  0x0806U
#define ETH_TYPE_IP   0x0800U
#define ARP_OP_REQ    0x0001U
#define ARP_OP_REP    0x0002U
#define IP_PROTO_ICMP 1U
#define IP_PROTO_TCP  6U
#define ICMP_ECHO_REQ 8U
#define ICMP_ECHO_REP 0U

#define TCP_FIN 0x01U
#define TCP_SYN 0x02U
#define TCP_RST 0x04U
#define TCP_PSH 0x08U
#define TCP_ACK 0x10U

#define HTONS(x) ((uint16_t)((((x) & 0xFFU) << 8) | (((x) >> 8) & 0xFFU)))
#define HTONL(x) ((uint32_t)((((x) & 0xFFU) << 24) | (((x) & 0xFF00U) << 8) | \
                     (((x) & 0xFF0000U) >> 8) | (((x) >> 24) & 0xFFU)))

typedef struct
{
  uint8_t buffer[APP_NET_RX_BUF_SIZE];
  uint8_t used;
} AppNetRxBuf_t;

typedef struct
{
  uint8_t *data;
  uint16_t len;
} AppNetRxFrame_t;

typedef enum
{
  TCP_CLOSED = 0,
  TCP_LISTEN,
  TCP_SYN_RCVD,
  TCP_ESTABLISHED
} AppNetTcpState_t;

static ETH_HandleTypeDef *s_heth;
static ETH_TxPacketConfig *s_tx_cfg;
static void (*s_log)(const char *msg);

static uint8_t s_mac[6];
static const uint8_t s_ip[4] = {192U, 168U, 10U, 2U};
static AppNetRxBuf_t s_rx_pool[APP_NET_RX_POOL]
  __attribute__((aligned(4), section(".RxArraySection")));
static uint8_t s_tx_buf[APP_NET_TX_BUF_SIZE]
  __attribute__((aligned(4), section(".RxArraySection")));

static AppNetRxFrame_t s_rx_frame;
static uint8_t s_link_up;
static uint32_t s_last_link_check;

static AppNetTcpState_t s_tcp_state = TCP_LISTEN;
static uint8_t s_peer_mac[6];
static uint8_t s_peer_ip[4];
static uint16_t s_peer_port;
static uint32_t s_tcp_iss = 1U;
static uint32_t s_tcp_snd_nxt;
static uint32_t s_tcp_rcv_nxt;
static uint32_t s_last_hello_ms;
static AppNetTelemetry_t s_telem;
static uint8_t s_telem_valid;

static void log_msg(const char *m)
{
  if (s_log != NULL)
  {
    s_log(m);
  }
}

static uint16_t checksum(const void *data, uint16_t len)
{
  const uint8_t *p = (const uint8_t *)data;
  uint32_t sum = 0U;
  while (len > 1U)
  {
    sum += (uint32_t)((uint16_t)p[0] << 8) | p[1];
    p += 2U;
    len = (uint16_t)(len - 2U);
  }
  if (len != 0U)
  {
    sum += (uint32_t)p[0] << 8;
  }
  while (sum >> 16)
  {
    sum = (sum & 0xFFFFU) + (sum >> 16);
  }
  return (uint16_t)(~sum);
}

static uint16_t tcp_checksum(const uint8_t *src_ip, const uint8_t *dst_ip,
                             const uint8_t *tcp, uint16_t tcp_len)
{
  uint32_t sum = 0U;
  uint16_t i;

  for (i = 0U; i < 4U; i += 2U)
  {
    sum += (uint32_t)((uint16_t)src_ip[i] << 8) | src_ip[i + 1U];
    sum += (uint32_t)((uint16_t)dst_ip[i] << 8) | dst_ip[i + 1U];
  }
  sum += IP_PROTO_TCP;
  sum += tcp_len;

  for (i = 0U; i + 1U < tcp_len; i += 2U)
  {
    sum += (uint32_t)((uint16_t)tcp[i] << 8) | tcp[i + 1U];
  }
  if ((tcp_len & 1U) != 0U)
  {
    sum += (uint32_t)tcp[tcp_len - 1U] << 8;
  }
  while (sum >> 16)
  {
    sum = (sum & 0xFFFFU) + (sum >> 16);
  }
  return (uint16_t)(~sum);
}

static int eth_tx(const uint8_t *frame, uint16_t len)
{
  ETH_BufferTypeDef txbuf = {0};
  ETH_TxPacketConfig cfg;

  if ((s_heth == NULL) || (s_tx_cfg == NULL) || (frame == NULL) || (len == 0U))
  {
    return -1;
  }

  memcpy(s_tx_buf, frame, len);
  if (len < 60U)
  {
    memset(&s_tx_buf[len], 0, (size_t)(60U - len));
    len = 60U;
  }

  cfg = *s_tx_cfg;
  cfg.Length = len;
  cfg.TxBuffer = &txbuf;
  cfg.Attributes = ETH_TX_PACKETS_FEATURES_CRCPAD;
  cfg.CRCPadCtrl = ETH_CRC_PAD_INSERT;
  cfg.ChecksumCtrl = ETH_CHECKSUM_DISABLE;

  txbuf.buffer = s_tx_buf;
  txbuf.len = len;
  txbuf.next = NULL;

  if (HAL_ETH_Transmit(s_heth, &cfg, 50U) != HAL_OK)
  {
    return -2;
  }
  return 0;
}

static void send_arp_reply(const uint8_t *dst_mac, const uint8_t *dst_ip)
{
  uint8_t f[42];
  memcpy(&f[0], dst_mac, 6);
  memcpy(&f[6], s_mac, 6);
  f[12] = 0x08;
  f[13] = 0x06;
  f[14] = 0x00;
  f[15] = 0x01; /* HW eth */
  f[16] = 0x08;
  f[17] = 0x00; /* proto IPv4 */
  f[18] = 6;
  f[19] = 4;
  f[20] = 0x00;
  f[21] = 0x02; /* reply */
  memcpy(&f[22], s_mac, 6);
  memcpy(&f[28], s_ip, 4);
  memcpy(&f[32], dst_mac, 6);
  memcpy(&f[38], dst_ip, 4);
  (void)eth_tx(f, sizeof(f));
}

static void send_icmp_echo_reply(const uint8_t *frame, uint16_t frame_len)
{
  uint8_t out[APP_NET_TX_BUF_SIZE];
  uint16_t ip_total;
  uint16_t icmp_len;
  uint16_t csum;
  const uint8_t *ip = &frame[14];
  uint8_t ihl = (uint8_t)((ip[0] & 0x0FU) * 4U);

  if ((frame_len < 14U + ihl + 8U) || (ihl < 20U))
  {
    return;
  }

  ip_total = (uint16_t)(((uint16_t)ip[2] << 8) | ip[3]);
  if ((uint16_t)(14U + ip_total) > frame_len)
  {
    return;
  }
  icmp_len = (uint16_t)(ip_total - ihl);
  if ((14U + 20U + icmp_len) > sizeof(out))
  {
    return;
  }

  /* Ethernet */
  memcpy(&out[0], &frame[6], 6);
  memcpy(&out[6], s_mac, 6);
  out[12] = 0x08;
  out[13] = 0x00;

  /* IP header (no options) */
  out[14] = 0x45;
  out[15] = 0x00;
  out[16] = (uint8_t)((20U + icmp_len) >> 8);
  out[17] = (uint8_t)(20U + icmp_len);
  out[18] = ip[4];
  out[19] = ip[5];
  out[20] = 0x00;
  out[21] = 0x00;
  out[22] = 64;
  out[23] = IP_PROTO_ICMP;
  out[24] = 0;
  out[25] = 0;
  memcpy(&out[26], s_ip, 4);
  memcpy(&out[30], &ip[12], 4);
  csum = checksum(&out[14], 20U);
  out[24] = (uint8_t)(csum >> 8);
  out[25] = (uint8_t)csum;

  /* ICMP: copy request, flip type, recompute checksum */
  memcpy(&out[34], &ip[ihl], icmp_len);
  out[34] = ICMP_ECHO_REP;
  out[36] = 0;
  out[37] = 0;
  csum = checksum(&out[34], icmp_len);
  out[36] = (uint8_t)(csum >> 8);
  out[37] = (uint8_t)csum;

  (void)eth_tx(out, (uint16_t)(34U + icmp_len));
}

static void tcp_send(uint8_t flags, const uint8_t *payload, uint16_t plen,
                     uint32_t seq, uint32_t ack)
{
  uint8_t out[APP_NET_TX_BUF_SIZE];
  uint16_t tcp_len = (uint16_t)(20U + plen);
  uint16_t total = (uint16_t)(20U + tcp_len);
  uint16_t csum;
  uint16_t i;

  if ((34U + tcp_len) > sizeof(out))
  {
    return;
  }

  memcpy(&out[0], s_peer_mac, 6);
  memcpy(&out[6], s_mac, 6);
  out[12] = 0x08;
  out[13] = 0x00;

  out[14] = 0x45;
  out[15] = 0x00;
  out[16] = (uint8_t)(total >> 8);
  out[17] = (uint8_t)total;
  out[18] = 0x00;
  out[19] = 0x01;
  out[20] = 0x40;
  out[21] = 0x00;
  out[22] = 64;
  out[23] = IP_PROTO_TCP;
  out[24] = 0;
  out[25] = 0;
  memcpy(&out[26], s_ip, 4);
  memcpy(&out[30], s_peer_ip, 4);
  csum = checksum(&out[14], 20U);
  out[24] = (uint8_t)(csum >> 8);
  out[25] = (uint8_t)csum;

  out[34] = (uint8_t)(APP_NET_TCP_PORT >> 8);
  out[35] = (uint8_t)APP_NET_TCP_PORT;
  out[36] = (uint8_t)(s_peer_port >> 8);
  out[37] = (uint8_t)s_peer_port;
  out[38] = (uint8_t)(seq >> 24);
  out[39] = (uint8_t)(seq >> 16);
  out[40] = (uint8_t)(seq >> 8);
  out[41] = (uint8_t)seq;
  out[42] = (uint8_t)(ack >> 24);
  out[43] = (uint8_t)(ack >> 16);
  out[44] = (uint8_t)(ack >> 8);
  out[45] = (uint8_t)ack;
  out[46] = 0x50; /* data offset 5 */
  out[47] = flags;
  out[48] = 0x20; /* window */
  out[49] = 0x00;
  out[50] = 0;
  out[51] = 0;
  out[52] = 0;
  out[53] = 0;

  if ((payload != NULL) && (plen != 0U))
  {
    memcpy(&out[54], payload, plen);
  }

  csum = tcp_checksum(s_ip, s_peer_ip, &out[34], tcp_len);
  out[50] = (uint8_t)(csum >> 8);
  out[51] = (uint8_t)csum;

  (void)i;
  (void)eth_tx(out, (uint16_t)(34U + tcp_len));
}

static void tcp_send_telemetry(void)
{
  char line[220];
  int n;
  int t10;
  int h10;
  int t_abs;
  const char *zone = (s_telem.zone[0] != '\0') ? s_telem.zone : "—";

  t10 = (int)(s_telem.temp_c * 10.0f);
  h10 = (int)(s_telem.hum_pct * 10.0f);
  t_abs = (t10 < 0) ? -t10 : t10;

  /* Newline JSON — Jetson AIR MONITOR / jetson_hmi.py */
  n = snprintf(line, sizeof(line),
               "{\"ok\":%u,\"temp_c\":%d.%d,\"hum_pct\":%d.%d,"
               "\"distance_cm\":%ld,\"cpu\":%d,\"heap\":%d,"
               "\"link_eth\":1,\"link_nfc\":%u,\"link_f401\":%u,"
               "\"freertos\":1,\"watchdog\":1,\"zone\":\"%s\"}\n",
               (unsigned)s_telem.ok,
               t10 / 10, t_abs % 10,
               h10 / 10, ((h10 < 0) ? -h10 : h10) % 10,
               (long)s_telem.distance_cm,
               (int)s_telem.cpu_pct,
               (int)s_telem.heap_pct,
               (unsigned)s_telem.link_nfc,
               (unsigned)s_telem.link_f401,
               zone);
  if (n < 1)
  {
    return;
  }
  if (n >= (int)sizeof(line))
  {
    n = (int)sizeof(line) - 1;
  }

  tcp_send(TCP_PSH | TCP_ACK, (const uint8_t *)line, (uint16_t)n,
           s_tcp_snd_nxt, s_tcp_rcv_nxt);
  s_tcp_snd_nxt += (uint32_t)n;
}

static void handle_tcp(const uint8_t *frame, uint16_t frame_len)
{
  const uint8_t *ip = &frame[14];
  uint8_t ihl = (uint8_t)((ip[0] & 0x0FU) * 4U);
  const uint8_t *tcp;
  uint16_t ip_total;
  uint16_t tcp_len;
  uint16_t dst_port;
  uint16_t src_port;
  uint32_t seq;
  uint32_t ack;
  uint8_t flags;
  uint8_t data_off;
  uint16_t payload_len;

  if ((frame_len < 14U + ihl + 20U) || (ihl < 20U))
  {
    return;
  }

  ip_total = (uint16_t)(((uint16_t)ip[2] << 8) | ip[3]);
  if ((uint16_t)(14U + ip_total) > frame_len)
  {
    return;
  }

  tcp = &ip[ihl];
  tcp_len = (uint16_t)(ip_total - ihl);
  if (tcp_len < 20U)
  {
    return;
  }

  src_port = (uint16_t)(((uint16_t)tcp[0] << 8) | tcp[1]);
  dst_port = (uint16_t)(((uint16_t)tcp[2] << 8) | tcp[3]);
  if (dst_port != APP_NET_TCP_PORT)
  {
    return;
  }

  seq = ((uint32_t)tcp[4] << 24) | ((uint32_t)tcp[5] << 16) |
        ((uint32_t)tcp[6] << 8) | (uint32_t)tcp[7];
  ack = ((uint32_t)tcp[8] << 24) | ((uint32_t)tcp[9] << 16) |
        ((uint32_t)tcp[10] << 8) | (uint32_t)tcp[11];
  data_off = (uint8_t)((tcp[12] >> 4) * 4U);
  flags = tcp[13];
  if (data_off < 20U || data_off > tcp_len)
  {
    return;
  }
  payload_len = (uint16_t)(tcp_len - data_off);

  if ((flags & TCP_RST) != 0U)
  {
    s_tcp_state = TCP_LISTEN;
    return;
  }

  if ((s_tcp_state == TCP_LISTEN) && ((flags & TCP_SYN) != 0U) &&
      ((flags & TCP_ACK) == 0U))
  {
    memcpy(s_peer_mac, &frame[6], 6);
    memcpy(s_peer_ip, &ip[12], 4);
    s_peer_port = src_port;
    s_tcp_iss += 1000U;
    s_tcp_snd_nxt = s_tcp_iss;
    s_tcp_rcv_nxt = seq + 1U;
    tcp_send(TCP_SYN | TCP_ACK, NULL, 0U, s_tcp_snd_nxt, s_tcp_rcv_nxt);
    s_tcp_snd_nxt++;
    s_tcp_state = TCP_SYN_RCVD;
    log_msg("ETH TCP SYN\r\n");
    return;
  }

  if ((s_tcp_state == TCP_SYN_RCVD) && ((flags & TCP_ACK) != 0U))
  {
    if (ack == s_tcp_snd_nxt)
    {
      s_tcp_state = TCP_ESTABLISHED;
      s_last_hello_ms = HAL_GetTick();
      log_msg("ETH TCP UP\r\n");
      tcp_send_telemetry();
    }
    return;
  }

  if (s_tcp_state == TCP_ESTABLISHED)
  {
    if ((flags & TCP_FIN) != 0U)
    {
      s_tcp_rcv_nxt = seq + 1U + payload_len;
      tcp_send(TCP_FIN | TCP_ACK, NULL, 0U, s_tcp_snd_nxt, s_tcp_rcv_nxt);
      s_tcp_snd_nxt++;
      s_tcp_state = TCP_LISTEN;
      log_msg("ETH TCP DOWN\r\n");
      return;
    }

    if (payload_len > 0U)
    {
      s_tcp_rcv_nxt = seq + payload_len;
      tcp_send(TCP_ACK, NULL, 0U, s_tcp_snd_nxt, s_tcp_rcv_nxt);
    }
    else if ((flags & TCP_ACK) != 0U)
    {
      /* keep-alive / pure ACK */
      (void)ack;
    }
  }
}

static void handle_frame(uint8_t *frame, uint16_t len)
{
  uint16_t etype;

  if ((frame == NULL) || (len < 14U))
  {
    return;
  }

  /* Strip FCS if present (DMA may include 4 CRC bytes) */
  if (len >= 18U)
  {
    /* HAL usually reports length without CRC for F7; keep as-is */
  }

  etype = (uint16_t)(((uint16_t)frame[12] << 8) | frame[13]);

  if (etype == ETH_TYPE_ARP)
  {
    if (len >= 42U)
    {
      uint16_t op = (uint16_t)(((uint16_t)frame[20] << 8) | frame[21]);
      if ((op == ARP_OP_REQ) && (memcmp(&frame[38], s_ip, 4) == 0))
      {
        send_arp_reply(&frame[22], &frame[28]);
      }
    }
    return;
  }

  if (etype == ETH_TYPE_IP)
  {
    const uint8_t *ip = &frame[14];
    uint8_t ihl;
    if (len < 34U)
    {
      return;
    }
    if (memcmp(&ip[16], s_ip, 4) != 0)
    {
      return;
    }
    ihl = (uint8_t)((ip[0] & 0x0FU) * 4U);
    if (ip[9] == IP_PROTO_ICMP)
    {
      if ((len >= (uint16_t)(14U + ihl + 1U)) && (ip[ihl] == ICMP_ECHO_REQ))
      {
        send_icmp_echo_reply(frame, len);
      }
    }
    else if (ip[9] == IP_PROTO_TCP)
    {
      handle_tcp(frame, len);
    }
  }
}

void HAL_ETH_RxAllocateCallback(uint8_t **buff)
{
  uint32_t i;
  *buff = NULL;
  for (i = 0U; i < APP_NET_RX_POOL; i++)
  {
    if (s_rx_pool[i].used == 0U)
    {
      s_rx_pool[i].used = 1U;
      *buff = s_rx_pool[i].buffer;
      return;
    }
  }
}

void HAL_ETH_RxLinkCallback(void **pStart, void **pEnd, uint8_t *buff, uint16_t Length)
{
  if (*pStart == NULL)
  {
    s_rx_frame.data = buff;
    s_rx_frame.len = Length;
    *pStart = &s_rx_frame;
    *pEnd = &s_rx_frame;
  }
  else
  {
    /* Multi-buffer frames not used; keep last length from LS descriptor */
    s_rx_frame.len = Length;
  }
}

void HAL_ETH_TxFreeCallback(uint32_t *buff)
{
  (void)buff;
}

static void rx_release(uint8_t *buff)
{
  uint32_t i;
  for (i = 0U; i < APP_NET_RX_POOL; i++)
  {
    if (s_rx_pool[i].buffer == buff)
    {
      s_rx_pool[i].used = 0U;
      return;
    }
  }
}

static void check_link(void)
{
  uint32_t now = HAL_GetTick();
  uint32_t phyreg = 0U;
  uint8_t up;

  if ((now - s_last_link_check) < 500U)
  {
    return;
  }
  s_last_link_check = now;

  /* LAN8742 BMSR @ reg 1, link status bit 2 */
  if (HAL_ETH_ReadPHYRegister(s_heth, 0U, 1U, &phyreg) != HAL_OK)
  {
    return;
  }
  up = ((phyreg & (1UL << 2)) != 0U) ? 1U : 0U;
  if (up != s_link_up)
  {
    s_link_up = up;
    if (up != 0U)
    {
      log_msg("ETH link UP\r\n");
      (void)HAL_ETH_Start(s_heth);
    }
    else
    {
      log_msg("ETH link DOWN\r\n");
      s_tcp_state = TCP_LISTEN;
      (void)HAL_ETH_Stop(s_heth);
    }
  }
}

void AppNet_SetLog(void (*fn)(const char *msg))
{
  s_log = fn;
}

int AppNet_Init(ETH_HandleTypeDef *heth_p, ETH_TxPacketConfig *tx_cfg)
{
  uint32_t i;

  if ((heth_p == NULL) || (tx_cfg == NULL) || (heth_p->Init.MACAddr == NULL))
  {
    return -1;
  }

  s_heth = heth_p;
  s_tx_cfg = tx_cfg;
  memcpy(s_mac, heth_p->Init.MACAddr, 6);

  for (i = 0U; i < APP_NET_RX_POOL; i++)
  {
    s_rx_pool[i].used = 0U;
  }

  s_tcp_state = TCP_LISTEN;
  s_link_up = 0U;
  s_last_link_check = 0U;
  s_telem_valid = 0U;
  memset(&s_telem, 0, sizeof(s_telem));
  s_telem.distance_cm = -1;
  (void)snprintf(s_telem.zone, sizeof(s_telem.zone), "-");

  /* Build RX descriptors with buffers via allocate callback */
  if (HAL_ETH_Start(s_heth) != HAL_OK)
  {
    log_msg("ETH start fail\r\n");
    return -2;
  }

  log_msg("ETH 192.168.10.2:5000 ready\r\n");
  return 0;
}

void AppNet_Poll(void)
{
  void *pkt = NULL;
  AppNetRxFrame_t *rx;

  if (s_heth == NULL)
  {
    return;
  }

  check_link();

  while (HAL_ETH_ReadData(s_heth, &pkt) == HAL_OK)
  {
    rx = (AppNetRxFrame_t *)pkt;
    if ((rx != NULL) && (rx->data != NULL) && (rx->len >= 14U))
    {
      handle_frame(rx->data, rx->len);
      rx_release(rx->data);
    }
    pkt = NULL;
  }

  if ((s_tcp_state == TCP_ESTABLISHED) && s_link_up)
  {
    uint32_t now = HAL_GetTick();
    if ((now - s_last_hello_ms) >= 1000U)
    {
      s_last_hello_ms = now;
      tcp_send_telemetry();
    }
  }
}

void AppNet_SetTelemetry(const AppNetTelemetry_t *t)
{
  if (t == NULL)
  {
    return;
  }
  s_telem = *t;
  s_telem.zone[sizeof(s_telem.zone) - 1U] = '\0';
  s_telem_valid = 1U;
}

uint8_t AppNet_LinkUp(void)
{
  return s_link_up;
}

uint8_t AppNet_TcpConnected(void)
{
  return (s_tcp_state == TCP_ESTABLISHED) ? 1U : 0U;
}
