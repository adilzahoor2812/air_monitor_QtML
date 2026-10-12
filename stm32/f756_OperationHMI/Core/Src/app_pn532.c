/**
 * @file app_pn532.c
 * @brief PN532 — SPI (recommended), soft-UART D14/D15, USART6, I2C.
 *
 * Your logs (UART rx=0 + I2C no ACK) match SPI DIP mode.
 * Set DIP SW1=OFF SW2=ON, wire SPI pins below.
 */
#include "app_pn532.h"

#include <stdio.h>
#include <string.h>

#define PN532_I2C_ADDR       (0x24U << 1)
#define PN532_I2C_READY      0x01U
#define PN532_HOSTTOPN532    0xD4U
#define PN532_SPI_STATREAD   0x02U
#define PN532_SPI_DATAWRITE  0x01U
#define PN532_SPI_DATAREAD   0x03U
#define PN532_SPI_READY      0x01U

#define CMD_GETFIRMWAREVERSION  0x02U
#define CMD_SAMCONFIGURATION    0x14U
#define CMD_INLISTPASSIVETARGET 0x4AU
#define CMD_RFCONFIGURATION     0x32U

/* Soft SPI on Arduino D10/D11/D12/D13 (ETH disabled so D11/PA7 is free) */
#define SPI_SS_PORT   GPIOD
#define SPI_SS_PIN    GPIO_PIN_14 /* D10 */
#define SPI_SCK_PORT  GPIOA
#define SPI_SCK_PIN   GPIO_PIN_5  /* D13 */
#define SPI_MISO_PORT GPIOA
#define SPI_MISO_PIN  GPIO_PIN_6  /* D12 */
#define SPI_MOSI_PORT GPIOA
#define SPI_MOSI_PIN  GPIO_PIN_7  /* D11 — requires ETH not using PA7 */

#define SU_PORT    GPIOB
#define SU_PIN_D15 GPIO_PIN_8
#define SU_PIN_D14 GPIO_PIN_9
#define DWT_LAR (*((volatile uint32_t *)0xE0001FB0u))
#define SU_BAUD 115200U

static I2C_HandleTypeDef *s_hi2c;
static UART_HandleTypeDef *s_huart;
static AppPn532Bus_t s_bus;
static uint8_t s_ready;
static void (*s_log)(const char *s);
static uint32_t s_bit_cycles;
static uint16_t s_su_tx_pin;
static uint16_t s_su_rx_pin;
static uint8_t s_rx_hold;
static uint8_t s_rx_hold_valid;

static void wake_bytes(void);
static int bringup_protocol(void);

void AppPn532_SetLog(void (*log_fn)(const char *s))
{
  s_log = log_fn;
}

static void log_msg(const char *s)
{
  if (s_log != NULL)
  {
    s_log(s);
  }
}

void AppPn532_HwReset(void)
{
  /* D3/PE13 is TFT SPI4_MISO — do not pulse it as NFC RST. */
  HAL_Delay(20);
}

static void dwt_enable(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT_LAR = 0xC5ACCE55u;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  s_bit_cycles = SystemCoreClock / SU_BAUD;
  if (s_bit_cycles < 8U)
  {
    s_bit_cycles = 8U;
  }
}

static void delay_cycles(uint32_t cycles)
{
  const uint32_t start = DWT->CYCCNT;
  while ((DWT->CYCCNT - start) < cycles)
  {
  }
}

/* ===== Soft SPI (Mode 0, LSB first) ===== */
static uint8_t s_spi_lsb = 1U; /* 1=LSB first (PN532), 0=MSB first (try fallback) */

static void spi_gpio_init(void)
{
  GPIO_InitTypeDef gpio = {0};
  dwt_enable();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /* Free PA7 from Ethernet AF if it was configured */
  HAL_GPIO_DeInit(GPIOA, GPIO_PIN_7);

  HAL_GPIO_WritePin(SPI_SS_PORT, SPI_SS_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(SPI_SCK_PORT, SPI_SCK_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(SPI_MOSI_PORT, SPI_MOSI_PIN, GPIO_PIN_RESET);

  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

  gpio.Pin = SPI_SS_PIN;
  HAL_GPIO_Init(SPI_SS_PORT, &gpio);
  gpio.Pin = SPI_SCK_PIN;
  HAL_GPIO_Init(SPI_SCK_PORT, &gpio);
  gpio.Pin = SPI_MOSI_PIN;
  HAL_GPIO_Init(SPI_MOSI_PORT, &gpio);

  gpio.Pin = SPI_MISO_PIN;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(SPI_MISO_PORT, &gpio);
}

static void spi_ss(uint8_t low)
{
  HAL_GPIO_WritePin(SPI_SS_PORT, SPI_SS_PIN, low ? GPIO_PIN_RESET : GPIO_PIN_SET);
  delay_cycles(SystemCoreClock / 200000U); /* ~5 us */
}

static uint8_t spi_transfer(uint8_t out)
{
  uint8_t in = 0U;
  uint8_t bit;
  const uint32_t half = SystemCoreClock / 200000U; /* ~5 us half-period, slow */

  for (bit = 0U; bit < 8U; bit++)
  {
    uint8_t out_bit;
    if (s_spi_lsb != 0U)
    {
      out_bit = (uint8_t)(out & 1U);
      out = (uint8_t)(out >> 1);
    }
    else
    {
      out_bit = (uint8_t)((out & 0x80U) != 0U);
      out = (uint8_t)(out << 1);
    }

    HAL_GPIO_WritePin(SPI_MOSI_PORT, SPI_MOSI_PIN,
                      (out_bit != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    delay_cycles(half);
    HAL_GPIO_WritePin(SPI_SCK_PORT, SPI_SCK_PIN, GPIO_PIN_SET);
    delay_cycles(half);
    {
      uint8_t in_bit =
          (HAL_GPIO_ReadPin(SPI_MISO_PORT, SPI_MISO_PIN) != GPIO_PIN_RESET) ? 1U
                                                                            : 0U;
      if (s_spi_lsb != 0U)
      {
        in = (uint8_t)(in | (uint8_t)(in_bit << bit));
      }
      else
      {
        in = (uint8_t)((in << 1) | in_bit);
      }
    }
    HAL_GPIO_WritePin(SPI_SCK_PORT, SPI_SCK_PIN, GPIO_PIN_RESET);
    delay_cycles(half);
  }
  return in;
}

static int spi_isready(void)
{
  uint8_t cmd = PN532_SPI_STATREAD;
  uint8_t reply;
  spi_ss(1);
  (void)spi_transfer(cmd);
  reply = spi_transfer(0x00U);
  spi_ss(0);
  return (reply == PN532_SPI_READY) ? 1 : 0;
}

static int spi_waitready(uint32_t timeout_ms)
{
  const uint32_t t0 = HAL_GetTick();
  while (spi_isready() == 0)
  {
    if ((HAL_GetTick() - t0) > timeout_ms)
    {
      return -1;
    }
    HAL_Delay(5);
  }
  return 0;
}

/* ===== Soft UART ===== */
static void suart_gpio_init(uint16_t tx_pin, uint16_t rx_pin)
{
  GPIO_InitTypeDef gpio = {0};
  __HAL_RCC_GPIOB_CLK_ENABLE();
  dwt_enable();
  s_su_tx_pin = tx_pin;
  s_su_rx_pin = rx_pin;
  HAL_GPIO_WritePin(SU_PORT, s_su_tx_pin, GPIO_PIN_SET);
  gpio.Pin = s_su_tx_pin;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(SU_PORT, &gpio);
  gpio.Pin = s_su_rx_pin;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(SU_PORT, &gpio);
}

static void suart_write_byte(uint8_t b)
{
  uint8_t i;
  __disable_irq();
  HAL_GPIO_WritePin(SU_PORT, s_su_tx_pin, GPIO_PIN_RESET);
  delay_cycles(s_bit_cycles);
  for (i = 0U; i < 8U; i++)
  {
    HAL_GPIO_WritePin(SU_PORT, s_su_tx_pin,
                      ((b & 1U) != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    b = (uint8_t)(b >> 1);
    delay_cycles(s_bit_cycles);
  }
  HAL_GPIO_WritePin(SU_PORT, s_su_tx_pin, GPIO_PIN_SET);
  delay_cycles(s_bit_cycles);
  __enable_irq();
}

static int suart_read_byte(uint8_t *out, uint32_t timeout_ms)
{
  uint32_t t0 = HAL_GetTick();
  uint8_t b = 0U;
  uint8_t i;
  while (HAL_GPIO_ReadPin(SU_PORT, s_su_rx_pin) != GPIO_PIN_RESET)
  {
    if ((HAL_GetTick() - t0) > timeout_ms)
    {
      return -1;
    }
  }
  __disable_irq();
  delay_cycles(s_bit_cycles + (s_bit_cycles / 2U));
  for (i = 0U; i < 8U; i++)
  {
    if (HAL_GPIO_ReadPin(SU_PORT, s_su_rx_pin) != GPIO_PIN_RESET)
    {
      b = (uint8_t)(b | (uint8_t)(1U << i));
    }
    delay_cycles(s_bit_cycles);
  }
  delay_cycles(s_bit_cycles);
  __enable_irq();
  *out = b;
  return 0;
}

static int suart_write(const uint8_t *data, uint16_t n)
{
  uint16_t i;
  for (i = 0U; i < n; i++)
  {
    suart_write_byte(data[i]);
  }
  return 0;
}

static int suart_read(uint8_t *data, uint16_t n, uint32_t first_timeout_ms)
{
  uint16_t i;
  for (i = 0U; i < n; i++)
  {
    if (suart_read_byte(&data[i], (i == 0U) ? first_timeout_ms : 50U) != 0)
    {
      return -1;
    }
  }
  return 0;
}

static void uart_hw_flush(void)
{
  uint8_t b;
  uint32_t guard = 0U;
  if (s_huart == NULL)
  {
    return;
  }
  while ((HAL_UART_Receive(s_huart, &b, 1U, 2U) == HAL_OK) && (guard++ < 64U))
  {
  }
}

static int wait_uart_byte(uint32_t timeout_ms)
{
  const uint32_t t0 = HAL_GetTick();
  while (__HAL_UART_GET_FLAG(s_huart, UART_FLAG_RXNE) == RESET)
  {
    if ((HAL_GetTick() - t0) > timeout_ms)
    {
      return -1;
    }
  }
  return 0;
}

static int isready_i2c(void)
{
  uint8_t rdy = 0U;
  if (HAL_I2C_Master_Receive(s_hi2c, PN532_I2C_ADDR, &rdy, 1U, 20U) != HAL_OK)
  {
    return 0;
  }
  return (rdy == PN532_I2C_READY) ? 1 : 0;
}

static int waitready_i2c(uint32_t timeout_ms)
{
  const uint32_t t0 = HAL_GetTick();
  while (isready_i2c() == 0)
  {
    if ((HAL_GetTick() - t0) > timeout_ms)
    {
      return -1;
    }
    HAL_Delay(10);
  }
  return 0;
}

static int build_frame(uint8_t *packet, const uint8_t *cmd, uint8_t cmdlen)
{
  uint8_t LEN = (uint8_t)(cmdlen + 1U);
  uint8_t i;
  uint8_t sum = 0U;
  packet[0] = 0x00U;
  packet[1] = 0x00U;
  packet[2] = 0xFFU;
  packet[3] = LEN;
  packet[4] = (uint8_t)(~LEN + 1U);
  packet[5] = PN532_HOSTTOPN532;
  for (i = 0U; i < cmdlen; i++)
  {
    packet[6U + i] = cmd[i];
    sum = (uint8_t)(sum + cmd[i]);
  }
  packet[6U + cmdlen] = (uint8_t)(~(PN532_HOSTTOPN532 + sum) + 1U);
  packet[7U + cmdlen] = 0x00U;
  return (int)(8U + cmdlen);
}

static int writecommand(const uint8_t *cmd, uint8_t cmdlen)
{
  uint8_t packet[48];
  int flen = build_frame(packet, cmd, cmdlen);

  if (s_bus == APP_PN532_BUS_SPI)
  {
    uint8_t i;
    spi_ss(1);
    (void)spi_transfer(PN532_SPI_DATAWRITE);
    for (i = 0U; i < (uint8_t)flen; i++)
    {
      (void)spi_transfer(packet[i]);
    }
    spi_ss(0);
    return 0;
  }
  if (s_bus == APP_PN532_BUS_I2C)
  {
    return (HAL_I2C_Master_Transmit(s_hi2c, PN532_I2C_ADDR, packet, (uint16_t)flen,
                                    100U) == HAL_OK)
               ? 0
               : -2;
  }
  if (s_bus == APP_PN532_BUS_UART)
  {
    return (HAL_UART_Transmit(s_huart, packet, (uint16_t)flen, 100U) == HAL_OK) ? 0
                                                                                : -2;
  }
  if (s_bus == APP_PN532_BUS_SUART)
  {
    return suart_write(packet, (uint16_t)flen);
  }
  return -3;
}

static int readdata(uint8_t *buff, uint8_t n)
{
  if (s_bus == APP_PN532_BUS_SPI)
  {
    uint8_t i;
    spi_ss(1);
    (void)spi_transfer(PN532_SPI_DATAREAD);
    for (i = 0U; i < n; i++)
    {
      buff[i] = spi_transfer(0x00U);
    }
    spi_ss(0);
    return 0;
  }
  if (s_bus == APP_PN532_BUS_I2C)
  {
    uint8_t tmp[48];
    uint8_t i;
    if (HAL_I2C_Master_Receive(s_hi2c, PN532_I2C_ADDR, tmp, (uint16_t)(n + 1U),
                               100U) != HAL_OK)
    {
      return -2;
    }
    for (i = 0U; i < n; i++)
    {
      buff[i] = tmp[1U + i];
    }
    return 0;
  }
  if (s_bus == APP_PN532_BUS_UART)
  {
    return (HAL_UART_Receive(s_huart, buff, n, 800U) == HAL_OK) ? 0 : -2;
  }
  if (s_bus == APP_PN532_BUS_SUART)
  {
    uint8_t i = 0U;
    if (s_rx_hold_valid != 0U)
    {
      buff[0] = s_rx_hold;
      s_rx_hold_valid = 0U;
      i = 1U;
    }
    if (i < n)
    {
      return suart_read(&buff[i], (uint16_t)(n - i), 800U);
    }
    return 0;
  }
  return -3;
}

static int readack(void)
{
  uint8_t ackbuff[6];
  static const uint8_t pn532ack[6] = {
      0x00U, 0x00U, 0xFFU, 0x00U, 0xFFU, 0x00U};
  if (readdata(ackbuff, 6U) != 0)
  {
    return -1;
  }
  return (memcmp(ackbuff, pn532ack, 6) == 0) ? 0 : -2;
}

static int sendCommandCheckAck(const uint8_t *cmd, uint8_t cmdlen,
                               uint32_t timeout_ms)
{
  if (writecommand(cmd, cmdlen) != 0)
  {
    return -1;
  }
  HAL_Delay(2);

  if (s_bus == APP_PN532_BUS_SPI)
  {
    if (spi_waitready(timeout_ms) != 0)
    {
      return -2;
    }
    if (readack() != 0)
    {
      return -3;
    }
    HAL_Delay(2);
    if (spi_waitready(timeout_ms) != 0)
    {
      return -4;
    }
    return 0;
  }

  if (s_bus == APP_PN532_BUS_I2C)
  {
    if (waitready_i2c(timeout_ms) != 0)
    {
      return -2;
    }
    if (readack() != 0)
    {
      return -3;
    }
    HAL_Delay(2);
    if (waitready_i2c(timeout_ms) != 0)
    {
      return -4;
    }
    return 0;
  }

  if (s_bus == APP_PN532_BUS_UART)
  {
    if (wait_uart_byte(timeout_ms) != 0)
    {
      return -2;
    }
    if (readack() != 0)
    {
      return -3;
    }
    HAL_Delay(2);
    if (wait_uart_byte(timeout_ms) != 0)
    {
      return -4;
    }
    return 0;
  }

  if (s_bus == APP_PN532_BUS_SUART)
  {
    uint8_t ackbuff[6];
    static const uint8_t pn532ack[6] = {
        0x00U, 0x00U, 0xFFU, 0x00U, 0xFFU, 0x00U};
    s_rx_hold_valid = 0U;
    if (suart_read(ackbuff, 6U, timeout_ms) != 0)
    {
      return -2;
    }
    if (memcmp(ackbuff, pn532ack, 6) != 0)
    {
      return -3;
    }
    HAL_Delay(2);
    if (suart_read_byte(&s_rx_hold, timeout_ms) != 0)
    {
      return -4;
    }
    s_rx_hold_valid = 1U;
    return 0;
  }
  return -5;
}

static int packet_has_rsp(const uint8_t *packet, uint8_t len, uint8_t code)
{
  uint8_t i;
  for (i = 0U; (uint8_t)(i + 1U) < len; i++)
  {
    if ((packet[i] == 0xD5U) && (packet[i + 1U] == code))
    {
      return 1;
    }
  }
  return 0;
}

static int bringup_protocol(void)
{
  uint8_t cmd[5];
  uint8_t packet[16];

  cmd[0] = CMD_GETFIRMWAREVERSION;
  if (sendCommandCheckAck(cmd, 1U, 1500U) != 0)
  {
    return -2;
  }
  if (readdata(packet, 13U) != 0)
  {
    return -3;
  }
  if ((packet[0] != 0x00U || packet[2] != 0xFFU) &&
      (packet_has_rsp(packet, 13U, 0x03U) == 0))
  {
    return -4;
  }

  cmd[0] = CMD_SAMCONFIGURATION;
  cmd[1] = 0x01U;
  cmd[2] = 0x14U;
  cmd[3] = 0x01U;
  if (sendCommandCheckAck(cmd, 4U, 1500U) != 0)
  {
    return -5;
  }
  if (readdata(packet, 9U) != 0)
  {
    return -6;
  }
  if ((packet[6] != 0x15U) && (packet_has_rsp(packet, 9U, 0x15U) == 0))
  {
    return -7;
  }

  cmd[0] = CMD_RFCONFIGURATION;
  cmd[1] = 0x05U;
  cmd[2] = 0xFFU;
  cmd[3] = 0x01U;
  cmd[4] = 0x02U;
  if (sendCommandCheckAck(cmd, 5U, 1000U) == 0)
  {
    (void)readdata(packet, 9U);
  }
  return 0;
}

static void wake_bytes(void)
{
  static const uint8_t wake[] = {0x55U, 0x55U, 0x00U, 0x00U, 0x00U};
  if (s_bus == APP_PN532_BUS_SUART)
  {
    (void)suart_write(wake, sizeof(wake));
    HAL_Delay(50);
  }
  else if (s_bus == APP_PN532_BUS_UART)
  {
    uart_hw_flush();
    (void)HAL_UART_Transmit(s_huart, (uint8_t *)wake, sizeof(wake), 100U);
    HAL_Delay(50);
    uart_hw_flush();
  }
}

static int try_spi(void)
{
  char line[72];
  uint8_t i;
  uint8_t st;
  uint8_t attempt;

  log_msg("NFC trying SPI (DIP SW1=OFF SW2=ON)...\r\n");
  log_msg("  SS->D10 SCK->D13 MISO->D12 MOSI->D11  VCC->5V\r\n");
  spi_gpio_init();
  s_hi2c = NULL;
  s_huart = NULL;
  s_bus = APP_PN532_BUS_SPI;
  AppPn532_HwReset();

  log_msg("NFC_DIAG SPI status:");
  for (i = 0U; i < 8U; i++)
  {
    spi_ss(1);
    (void)spi_transfer(PN532_SPI_STATREAD);
    st = spi_transfer(0x00U);
    spi_ss(0);
    (void)snprintf(line, sizeof(line), " %02X", (unsigned)st);
    log_msg(line);
    HAL_Delay(5);
  }
  log_msg("\r\n");

  for (attempt = 0U; attempt < 2U; attempt++)
  {
    s_spi_lsb = (attempt == 0U) ? 1U : 0U;
    (void)snprintf(line, sizeof(line), "NFC_DIAG SPI bitorder=%s\r\n",
                   (s_spi_lsb != 0U) ? "LSB" : "MSB");
    log_msg(line);
    AppPn532_HwReset();
    if (bringup_protocol() == 0)
    {
      return 0;
    }
  }

  log_msg("NFC_DIAG SPI bringup failed\r\n");
  log_msg("Check pins SS/SCK/MISO/MOSI on module (not SDA/SCL)\r\n");
  s_bus = APP_PN532_BUS_NONE;
  return -30;
}

static uint8_t diag_suart_count(void)
{
  static const uint8_t wake[] = {0x55U, 0x55U, 0x00U, 0x00U, 0x00U};
  uint8_t cmd_pkt[] = {
      0x00U, 0x00U, 0xFFU, 0x02U, 0xFEU, 0xD4U, 0x02U, 0x2AU, 0x00U};
  uint8_t n = 0U;
  uint8_t b;
  uint32_t t0;
  char line[64];

  (void)suart_write(wake, sizeof(wake));
  HAL_Delay(50);
  (void)suart_write(cmd_pkt, sizeof(cmd_pkt));
  t0 = HAL_GetTick();
  while (((HAL_GetTick() - t0) < 300U) && (n < 24U))
  {
    if (suart_read_byte(&b, 5U) == 0)
    {
      n++;
    }
  }
  (void)snprintf(line, sizeof(line), "NFC_DIAG suart_rx=%u\r\n", (unsigned)n);
  log_msg(line);
  return n;
}

static int try_suart_orientation(uint16_t tx_pin, uint16_t rx_pin, const char *tag)
{
  char line[64];
  (void)snprintf(line, sizeof(line), "NFC soft-UART %s\r\n", tag);
  log_msg(line);
  suart_gpio_init(tx_pin, rx_pin);
  s_bus = APP_PN532_BUS_SUART;
  AppPn532_HwReset();
  if (diag_suart_count() == 0U)
  {
    s_bus = APP_PN532_BUS_NONE;
    return -21;
  }
  AppPn532_HwReset();
  wake_bytes();
  if (bringup_protocol() != 0)
  {
    s_bus = APP_PN532_BUS_NONE;
    return -20;
  }
  return 0;
}

static int try_suart(I2C_HandleTypeDef *hi2c)
{
  log_msg("NFC trying soft-UART D14/D15...\r\n");
  if (hi2c != NULL)
  {
    (void)HAL_I2C_DeInit(hi2c);
  }
  if (try_suart_orientation(SU_PIN_D15, SU_PIN_D14, "TX=D15 RX=D14") == 0)
  {
    return 0;
  }
  if (try_suart_orientation(SU_PIN_D14, SU_PIN_D15, "TX=D14 RX=D15") == 0)
  {
    return 0;
  }
  return -20;
}

static int try_uart6(UART_HandleTypeDef *huart)
{
  log_msg("NFC trying USART6 D0/D1...\r\n");
  s_huart = huart;
  s_bus = APP_PN532_BUS_UART;
  AppPn532_HwReset();
  wake_bytes();
  if (bringup_protocol() != 0)
  {
    s_bus = APP_PN532_BUS_NONE;
    return -12;
  }
  return 0;
}

static int try_i2c(I2C_HandleTypeDef *hi2c)
{
  log_msg("NFC trying I2C (DIP SW1=ON SW2=OFF)...\r\n");
  if (HAL_I2C_Init(hi2c) != HAL_OK)
  {
    return -9;
  }
  s_hi2c = hi2c;
  s_bus = APP_PN532_BUS_I2C;
  AppPn532_HwReset();
  if (HAL_I2C_IsDeviceReady(s_hi2c, PN532_I2C_ADDR, 3U, 50U) != HAL_OK)
  {
    log_msg("NFC_DIAG I2C no ACK @0x24\r\n");
    s_bus = APP_PN532_BUS_NONE;
    return -10;
  }
  if (bringup_protocol() != 0)
  {
    s_bus = APP_PN532_BUS_NONE;
    return -2;
  }
  return 0;
}

int AppPn532_Init(I2C_HandleTypeDef *hi2c, UART_HandleTypeDef *huart)
{
  s_ready = 0U;
  s_bus = APP_PN532_BUS_NONE;
  s_rx_hold_valid = 0U;

  /* Soft-SPI uses PA7/D11 — reserved for ETH_CRS_DV when Ethernet is enabled.
   * Prefer UART / soft-UART / I2C so RMII keeps PA7. */
#if !defined(APP_PN532_ALLOW_SPI_ON_PA7)
  log_msg("NFC SPI skipped (PA7=ETH); try UART/I2C\r\n");
#else
  if (try_spi() == 0)
  {
    s_ready = 1U;
    return 0;
  }
#endif

  if (try_suart(hi2c) == 0)
  {
    s_ready = 1U;
    return 0;
  }

  if ((huart != NULL) && (try_uart6(huart) == 0))
  {
    s_ready = 1U;
    return 0;
  }

  if ((hi2c != NULL) && (try_i2c(hi2c) == 0))
  {
    s_ready = 1U;
    return 0;
  }

  return -1;
}

int AppPn532_IsReady(void)
{
  return (s_ready != 0U) ? 1 : 0;
}

AppPn532Bus_t AppPn532_GetBus(void)
{
  return s_bus;
}

int AppPn532_PollUid(AppPn532Uid_t *out)
{
  uint8_t cmd[3];
  uint8_t packet[24];
  uint8_t uid_len;
  uint8_t i;

  if ((out == NULL) || (s_ready == 0U))
  {
    return -1;
  }
  memset(out, 0, sizeof(*out));

  cmd[0] = CMD_INLISTPASSIVETARGET;
  cmd[1] = 0x01U;
  cmd[2] = 0x00U;

  if (sendCommandCheckAck(cmd, 3U, 150U) != 0)
  {
    return 1;
  }
  if (readdata(packet, 20U) != 0)
  {
    return 1;
  }
  if ((packet[6] != 0x4BU) || (packet[7] != 1U))
  {
    return 1;
  }
  uid_len = packet[12];
  if ((uid_len == 0U) || (uid_len > APP_PN532_UID_MAX))
  {
    return -3;
  }
  out->uid_len = uid_len;
  for (i = 0U; i < uid_len; i++)
  {
    out->uid[i] = packet[13U + i];
  }
  return 0;
}
