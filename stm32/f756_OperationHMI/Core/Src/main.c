/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "string.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "app_dht22.h"
#include "app_hcsr04.h"
#include "app_pn532.h"
#include "app_mem.h"
#include "app_tft.h"
#include "app_lvgl.h"
#include "app_net.h"
#include "operation_hmi.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
#if defined ( __ICCARM__ ) /*!< IAR Compiler */
#pragma location=0x2004c000
ETH_DMADescTypeDef  DMARxDscrTab[ETH_RX_DESC_CNT]; /* Ethernet Rx DMA Descriptors */
#pragma location=0x2004c0a0
ETH_DMADescTypeDef  DMATxDscrTab[ETH_TX_DESC_CNT]; /* Ethernet Tx DMA Descriptors */

#elif defined ( __CC_ARM )  /* MDK ARM Compiler */

__attribute__((at(0x2004c000))) ETH_DMADescTypeDef  DMARxDscrTab[ETH_RX_DESC_CNT]; /* Ethernet Rx DMA Descriptors */
__attribute__((at(0x2004c0a0))) ETH_DMADescTypeDef  DMATxDscrTab[ETH_TX_DESC_CNT]; /* Ethernet Tx DMA Descriptors */

#elif defined ( __GNUC__ ) /* GNU Compiler */

ETH_DMADescTypeDef DMARxDscrTab[ETH_RX_DESC_CNT] __attribute__((section(".RxDecripSection"))); /* Ethernet Rx DMA Descriptors */
ETH_DMADescTypeDef DMATxDscrTab[ETH_TX_DESC_CNT] __attribute__((section(".TxDecripSection")));   /* Ethernet Tx DMA Descriptors */
#endif

ETH_TxPacketConfig TxConfig;

ETH_HandleTypeDef heth;

I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart3;
UART_HandleTypeDef huart6;

PCD_HandleTypeDef hpcd_USB_OTG_FS;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_ETH_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_USART6_UART_Init(void);
static void MX_USB_OTG_FS_PCD_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#define NFC_SESSION_MS 30000U
#define NFC_POLL_MS    1000U

static char s_rx_line[48];
static uint8_t s_rx_len;
static volatile uint8_t s_ack_pending;
static uint32_t s_nfc_session_until;
static uint32_t s_nfc_last_poll;
static uint8_t s_nfc_armed_announced;
static uint8_t s_nfc_last_uid[APP_PN532_UID_MAX];
static uint8_t s_nfc_last_uid_len;
static uint8_t s_fault;     /* process alarm (DHT fail) */
static uint8_t s_fault_ack; /* 1 = quittiert, lamp steady until healthy */
static uint8_t s_ui_ready;
static int32_t s_distance_cm = -1;

static void on_hmi_ack(void)
{
  s_ack_pending = 1U;
}

static void hmi_uid_text(char *buf, size_t buf_sz)
{
  size_t off = 0U;
  uint8_t i;
  int n;

  if ((buf == NULL) || (buf_sz == 0U))
  {
    return;
  }
  buf[0] = '\0';
  if (s_nfc_last_uid_len == 0U)
  {
    (void)snprintf(buf, buf_sz, "LOCAL");
    return;
  }
  for (i = 0U; i < s_nfc_last_uid_len; i++)
  {
    n = snprintf(buf + off, buf_sz - off, "%s%02X",
                 (i == 0U) ? "" : ":", (unsigned)s_nfc_last_uid[i]);
    if ((n < 0) || ((size_t)n >= (buf_sz - off)))
    {
      break;
    }
    off += (size_t)n;
  }
}

static void uart3_print(const char *s)
{
  const uint16_t n = (uint16_t)strlen(s);
  if (HAL_UART_Transmit(&huart3, (uint8_t *)s, n, 1000) == HAL_OK)
  {
    HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin); /* blue = UART TX ok */
  }
}

static void update_fault_lamp(uint8_t flash)
{
  if (s_fault == 0U)
  {
    HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_RESET);
    return;
  }
  if (s_fault_ack != 0U)
  {
    HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_SET); /* steady = acknowledged */
    return;
  }
  if (flash != 0U)
  {
    HAL_GPIO_TogglePin(LD3_GPIO_Port, LD3_Pin); /* flash = unacknowledged */
  }
}

static void print_ack_ok(void)
{
  char line[96];
  size_t off = 0U;
  uint8_t i;
  int n;

  n = snprintf(line, sizeof(line), "ACK_OK t=%lu uid=",
               (unsigned long)HAL_GetTick());
  if (n < 0)
  {
    uart3_print("ACK_OK uid=NONE pri=NORMAL\r\n");
    return;
  }
  off = (size_t)n;
  if (s_nfc_last_uid_len == 0U)
  {
    n = snprintf(line + off, sizeof(line) - off, "NONE");
    if (n > 0)
    {
      off += (size_t)n;
    }
  }
  else
  {
    for (i = 0U; i < s_nfc_last_uid_len; i++)
    {
      n = snprintf(line + off, sizeof(line) - off, "%s%02X",
                   (i == 0U) ? "" : ":", (unsigned)s_nfc_last_uid[i]);
      if ((n < 0) || ((size_t)n >= (sizeof(line) - off)))
      {
        break;
      }
      off += (size_t)n;
    }
  }
  (void)snprintf(line + off, sizeof(line) - off, " pri=%s\r\n",
                 (s_fault != 0U) ? "ALARM" : "NORMAL");
  uart3_print(line);
}

static int nfc_session_active(void)
{
  return (HAL_GetTick() < s_nfc_session_until) ? 1 : 0;
}

static void format_uid_line(char *line, size_t line_sz, const AppPn532Uid_t *uid)
{
  size_t off = 0U;
  uint8_t i;
  int n;

  n = snprintf(line, line_sz, "NFC_UID=");
  if (n < 0)
  {
    line[0] = '\0';
    return;
  }
  off = (size_t)n;
  for (i = 0U; i < uid->uid_len; i++)
  {
    n = snprintf(line + off, line_sz - off, "%s%02X",
                 (i == 0U) ? "" : ":", (unsigned)uid->uid[i]);
    if ((n < 0) || ((size_t)n >= (line_sz - off)))
    {
      break;
    }
    off += (size_t)n;
  }
  (void)snprintf(line + off, line_sz - off, "\r\n");
}

static void service_nfc(void)
{
  AppPn532Uid_t uid;
  char line[64];
  int rc;
  const uint32_t now = HAL_GetTick();

  if (AppPn532_IsReady() == 0)
  {
    return;
  }

  if ((now - s_nfc_last_poll) < NFC_POLL_MS)
  {
    return;
  }
  s_nfc_last_poll = now;

  rc = AppPn532_PollUid(&uid);
  if (rc == 0)
  {
    const uint8_t same =
        (uid.uid_len == s_nfc_last_uid_len) &&
        (memcmp(uid.uid, s_nfc_last_uid, uid.uid_len) == 0);

    s_nfc_session_until = now + NFC_SESSION_MS;

    if (!same)
    {
      memcpy(s_nfc_last_uid, uid.uid, uid.uid_len);
      s_nfc_last_uid_len = uid.uid_len;
      format_uid_line(line, sizeof(line), &uid);
      uart3_print(line);
    }
    if (s_nfc_armed_announced == 0U)
    {
      uart3_print("NFC_ARMED\r\n");
      s_nfc_armed_announced = 1U;
    }
    if (s_ui_ready != 0U)
    {
      char uid[40];
      hmi_uid_text(uid, sizeof(uid));
      operation_hmi_set_operator_id(uid);
      operation_hmi_set_ack_armed(true);
      operation_hmi_set_link_nfc(true);
    }
  }
  else if ((s_nfc_armed_announced != 0U) && (nfc_session_active() == 0))
  {
    uart3_print("NFC_LOCKED\r\n");
    s_nfc_armed_announced = 0U;
    s_nfc_last_uid_len = 0U;
    if (s_ui_ready != 0U)
    {
      operation_hmi_set_ack_armed(false);
    }
  }
}

/* Called from USART3 IRQ for every RX byte (no HAL RX state machine). */
void AppUart3_OnRxByte(uint8_t ch)
{
  if ((ch == (uint8_t)'\r') || (ch == (uint8_t)'\n'))
  {
    if (s_rx_len > 0U)
    {
      s_rx_line[s_rx_len] = '\0';
      if (strcmp(s_rx_line, "ACK") == 0)
      {
        s_ack_pending = 1U;
      }
      s_rx_len = 0U;
    }
  }
  else if (s_rx_len < (uint8_t)(sizeof(s_rx_line) - 1U))
  {
    s_rx_line[s_rx_len++] = (char)ch;
  }
  else
  {
    s_rx_len = 0U;
  }
}

static void uart3_rx_irq_start(void)
{
  /* Clear leftover errors / data */
  if (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_ORE) != RESET)
  {
    __HAL_UART_CLEAR_OREFLAG(&huart3);
    (void)huart3.Instance->RDR;
  }
  __HAL_UART_ENABLE_IT(&huart3, UART_IT_RXNE);
}

static void service_ack(void)
{
  if (s_ack_pending == 0U)
  {
    return;
  }
  s_ack_pending = 0U;

  /* Badge required: tap NFC within NFC_SESSION_MS to unlock ACK */
  if ((AppPn532_IsReady() != 0) && (nfc_session_active() == 0))
  {
    uart3_print("ACK_DENIED\r\n");
    if (s_ui_ready != 0U)
    {
      operation_hmi_set_ack_status("DENIED");
      operation_hmi_confirm_ack(false, NULL);
    }
    return;
  }

  s_fault_ack = 1U;
  print_ack_ok();
  update_fault_lamp(0U);
  if (s_ui_ready != 0U)
  {
    char uid[40];
    hmi_uid_text(uid, sizeof(uid));
    operation_hmi_confirm_ack(true, uid);
  }
}

static void delay_ms_poll(uint32_t ms, uint32_t *busy_ms) __attribute__((unused));
static void delay_ms_poll(uint32_t ms, uint32_t *busy_ms)
{
  const uint32_t t0 = HAL_GetTick();
  uint32_t last_flash = t0;
  while ((HAL_GetTick() - t0) < ms)
  {
    const uint32_t b0 = HAL_GetTick();
    service_ack();
    service_nfc();
    if (busy_ms != NULL)
    {
      *busy_ms += (HAL_GetTick() - b0);
    }
    if ((HAL_GetTick() - last_flash) >= 200U)
    {
      last_flash = HAL_GetTick();
      update_fault_lamp(1U);
    }
    HAL_Delay(5);
  }
}

static uint32_t s_last_cpu;

static uint32_t heap_free_pct(void)
{
  uint32_t used = 0U;
  uint32_t total = 0U;
  uint32_t pct = 100U;

  AppMem_HeapStats(&used, &total);
  if (total > 0U)
  {
    pct = ((total - used) * 100U) / total;
    if (pct > 100U)
    {
      pct = 100U;
    }
  }
  return pct;
}

static void print_sysmon(uint32_t window_ms, uint32_t busy_ms)
{
  uint32_t cpu = 0U;
  char line[40];

  if (window_ms == 0U)
  {
    window_ms = 1U;
  }
  if (busy_ms > window_ms)
  {
    busy_ms = window_ms;
  }
  cpu = (busy_ms * 100U) / window_ms;
  if (cpu > 100U)
  {
    cpu = 100U;
  }
  s_last_cpu = cpu;

  /* nano.specs printf often ignores %lu — use %u so the HMI can parse the line. */
  (void)snprintf(line, sizeof(line), "CPU=%u HEAP=%u\r\n",
                 (unsigned)cpu, (unsigned)heap_free_pct());
  uart3_print(line);
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  /* ETH uses PA7 (RMII CRS_DV) — PN532 must not use soft-SPI on D11/PA7 */
  MX_ETH_Init();
  MX_I2C1_Init();
  MX_USART3_UART_Init();
  MX_USART6_UART_Init();
  MX_USB_OTG_FS_PCD_Init();
  /* USER CODE BEGIN 2 */
  {
    int nfc_rc;
    int eth_rc;

    uart3_print("\r\nF756 AIR MONITOR + DHT22 + HC-SR04 + PN532 + TFT + ETH\r\n");
    uart3_print("USART3 115200 | DHT22 on D2\r\n");
    uart3_print("HC-SR04 TRIG=D8 ECHO=D9 VCC=5V GND\r\n");
    uart3_print("ETH 192.168.10.2 TCP:5000 (Jetson 192.168.10.10/24)\r\n");
    uart3_print("TFT SPI4: CS D5  DC D7  RST D4  MOSI D38  SCK D39  LED D6\r\n");
    uart3_print("NFC: UART/I2C only (SPI D11/PA7 reserved for ETH)\r\n");

    AppHcsr04_SetLog(uart3_print);
    AppHcsr04_Init();
    uart3_print("HC-SR04 init done\r\n");

    AppTft_Init();
    uart3_print("TFT backlight ON (LED/D6 high)\r\n");
    AppTft_Fill(0xF800U);
    uart3_print("TFT fill red — if you see red, SPI is alive\r\n");

    AppNet_SetLog(uart3_print);
    eth_rc = AppNet_Init(&heth, &TxConfig);
    if (eth_rc != 0)
    {
      uart3_print("ETH=FAIL init\r\n");
    }

    AppPn532_SetLog(uart3_print);
    AppPn532_HwReset();

    AppDht22_Init();
    uart3_print("DHT22 init done\r\n");
    print_sysmon(1U, 0U);

    nfc_rc = AppPn532_Init(&hi2c1, &huart6);
    if (nfc_rc == 0)
    {
      if (AppPn532_GetBus() == APP_PN532_BUS_SPI)
      {
        uart3_print("NFC=OK SPI\r\n");
      }
      else if (AppPn532_GetBus() == APP_PN532_BUS_SUART)
      {
        uart3_print("NFC=OK UART D14/D15\r\n");
      }
      else if (AppPn532_GetBus() == APP_PN532_BUS_UART)
      {
        uart3_print("NFC=OK UART D0/D1\r\n");
      }
      else
      {
        uart3_print("NFC=OK I2C\r\n");
      }
    }
    else
    {
      uart3_print("NFC=FAIL\r\n");
      uart3_print("Paste NFC_DIAG SPI status: xx xx ... line\r\n");
      uart3_print("ACK works without badge until NFC is fixed\r\n");
    }

    HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_RESET);
    uart3_rx_irq_start();

    AppLvgl_Init();
    operation_hmi_set_ack_callback(on_hmi_ack);
    operation_hmi_create();
    s_ui_ready = 1U;
    operation_hmi_set_link_f756(true);
    operation_hmi_set_link_nfc(nfc_rc == 0);
    operation_hmi_set_link_eth(AppNet_LinkUp() != 0U);
    operation_hmi_set_distance_cm(-1);
    operation_hmi_set_status("F756 TFT + LVGL + ETH + SONAR");
    operation_hmi_log_event(OPERATION_HMI_EVT_OK, "Display online");
    uart3_print("LVGL HMI on TFT\r\n");
    (void)eth_rc;
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    {
      AppDht22Sample_t s;
      AppHcsr04Sample_t sonar;
      AppNetTelemetry_t tel;
      char line[96];
      int rc;
      int sonar_rc;
      float last_temp = 0.0f;
      float last_hum = 0.0f;
      uint8_t dht_ok = 0U;
      uint32_t busy_ms = 0U;
      uint32_t t_window = HAL_GetTick();
      uint32_t last_dht = 0U;
      uint32_t last_sonar = 0U;
      uint32_t last_good_sonar = 0U;
      uint32_t last_flash = HAL_GetTick();

      while (1)
      {
        const uint32_t now = HAL_GetTick();
        uint32_t b0;

        b0 = HAL_GetTick();
        AppNet_Poll();
        service_ack();
        service_nfc();
        busy_ms += (HAL_GetTick() - b0);

        AppLvgl_Handler();

        if (s_ui_ready != 0U)
        {
          operation_hmi_set_link_eth(AppNet_LinkUp() != 0U);
        }

        if ((now - last_flash) >= 200U)
        {
          last_flash = now;
          update_fault_lamp(1U);
        }

        /* HC-SR04 ~10 Hz (module needs ≥60 ms between pings) */
        if ((now - last_sonar) >= 100U)
        {
          last_sonar = now;
          b0 = HAL_GetTick();
          sonar_rc = AppHcsr04_Read(&sonar);
          busy_ms += (HAL_GetTick() - b0);
          if ((sonar_rc == 0) && (sonar.valid != 0U))
          {
            s_distance_cm = (int32_t)sonar.distance_cm;
            last_good_sonar = now;
            (void)snprintf(line, sizeof(line), "DIST=%lu cm echo=%lu us\r\n",
                           (unsigned long)sonar.distance_cm,
                           (unsigned long)sonar.echo_us);
            uart3_print(line);
            if (s_ui_ready != 0U)
            {
              operation_hmi_set_distance_cm((int)s_distance_cm);
              if (s_distance_cm < 20)
              {
                operation_hmi_set_zone("FAULT");
              }
              else if (s_distance_cm < 50)
              {
                operation_hmi_set_zone("WARN");
              }
              else if (dht_ok != 0U)
              {
                operation_hmi_set_zone("SAFE");
              }
            }
          }
          else
          {
            static uint32_t last_sonar_err_log;
            /* Keep last good distance on TFT/JSON for 2 s across glitches */
            if ((s_distance_cm < 0) ||
                (last_good_sonar == 0U) ||
                ((now - last_good_sonar) > 2000U))
            {
              s_distance_cm = -1;
              if (s_ui_ready != 0U)
              {
                operation_hmi_set_distance_cm(-1);
              }
            }
            if ((now - last_sonar_err_log) >= 1000U)
            {
              last_sonar_err_log = now;
              (void)snprintf(line, sizeof(line),
                             "SONAR fail err=%d (TRIG=D8 ECHO=D9 VCC=5V)\r\n",
                             AppHcsr04_LastError());
              uart3_print(line);
            }
          }

          memset(&tel, 0, sizeof(tel));
          tel.temp_c = last_temp;
          tel.hum_pct = last_hum;
          tel.distance_cm = s_distance_cm;
          tel.cpu_pct = (int16_t)s_last_cpu;
          tel.heap_pct = (int16_t)heap_free_pct();
          tel.ok = dht_ok;
          tel.link_nfc = (AppPn532_IsReady() != 0) ? 1U : 0U;
          if (s_distance_cm >= 0 && s_distance_cm < 20)
          {
            (void)snprintf(tel.zone, sizeof(tel.zone), "FAULT");
          }
          else if (s_distance_cm >= 0 && s_distance_cm < 50)
          {
            (void)snprintf(tel.zone, sizeof(tel.zone), "WARN");
          }
          else
          {
            (void)snprintf(tel.zone, sizeof(tel.zone), dht_ok ? "SAFE" : "FAULT");
          }
          AppNet_SetTelemetry(&tel);
        }

        if ((now - last_dht) < 2000U)
        {
          continue;
        }
        last_dht = now;

        b0 = HAL_GetTick();
        HAL_GPIO_TogglePin(LD1_GPIO_Port, LD1_Pin);
        uart3_print("DHT22 reading...\r\n");
        rc = AppDht22_Read(&s);
        service_ack();
        busy_ms += (HAL_GetTick() - b0);

        if ((rc == 0) && (s.valid != 0U))
        {
          const int t10 = (int)(s.temperature_c * 10.0f);
          const int h10 = (int)(s.humidity_pct * 10.0f);
          const int t_abs = (t10 < 0) ? -t10 : t10;
          (void)snprintf(line, sizeof(line),
                         "TEMP=%d.%d C  HUM=%d.%d %% DIST=%ld CPU=%u HEAP=%u\r\n",
                         t10 / 10, t_abs % 10,
                         h10 / 10, h10 % 10,
                         (long)s_distance_cm,
                         (unsigned)s_last_cpu, (unsigned)heap_free_pct());
          uart3_print(line);
          s_fault = 0U;
          s_fault_ack = 0U;
          dht_ok = 1U;
          last_temp = s.temperature_c;
          last_hum = s.humidity_pct;
          update_fault_lamp(0U);
          if (s_ui_ready != 0U)
          {
            operation_hmi_set_temp_c(s.temperature_c);
            operation_hmi_set_hum_pct(s.humidity_pct);
            operation_hmi_set_status("DHT22 OK");
          }
        }
        else
        {
          (void)snprintf(line, sizeof(line),
                         "DHT22 fail err=%d (OUT must be on D2=PF15)\r\n", rc);
          uart3_print(line);
          s_fault = 1U;
          dht_ok = 0U;
          update_fault_lamp(1U);
          if (s_ui_ready != 0U)
          {
            operation_hmi_set_zone("FAULT");
            operation_hmi_set_status("DHT22 fail");
          }
        }

        print_sysmon(HAL_GetTick() - t_window, busy_ms);
        if (s_ui_ready != 0U)
        {
          operation_hmi_set_cpu_pct((int)s_last_cpu);
          operation_hmi_set_heap_free_pct((int)heap_free_pct());
        }
        t_window = HAL_GetTick();
        busy_ms = 0U;
      }
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 72;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 3;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ETH Initialization Function
  * @param None
  * @retval None
  */
static void MX_ETH_Init(void)
{

  /* USER CODE BEGIN ETH_Init 0 */

  /* USER CODE END ETH_Init 0 */

   static uint8_t MACAddr[6];

  /* USER CODE BEGIN ETH_Init 1 */

  /* USER CODE END ETH_Init 1 */
  heth.Instance = ETH;
  MACAddr[0] = 0x00;
  MACAddr[1] = 0x80;
  MACAddr[2] = 0xE1;
  MACAddr[3] = 0x00;
  MACAddr[4] = 0x00;
  MACAddr[5] = 0x00;
  heth.Init.MACAddr = &MACAddr[0];
  heth.Init.MediaInterface = HAL_ETH_RMII_MODE;
  heth.Init.TxDesc = DMATxDscrTab;
  heth.Init.RxDesc = DMARxDscrTab;
  heth.Init.RxBuffLen = 1524;

  /* USER CODE BEGIN MACADDRESS */

  /* USER CODE END MACADDRESS */

  if (HAL_ETH_Init(&heth) != HAL_OK)
  {
    Error_Handler();
  }

  memset(&TxConfig, 0 , sizeof(ETH_TxPacketConfig));
  TxConfig.Attributes = ETH_TX_PACKETS_FEATURES_CSUM | ETH_TX_PACKETS_FEATURES_CRCPAD;
  TxConfig.ChecksumCtrl = ETH_CHECKSUM_IPHDR_PAYLOAD_INSERT_PHDR_CALC;
  TxConfig.CRCPadCtrl = ETH_CRC_PAD_INSERT;
  /* USER CODE BEGIN ETH_Init 2 */

  /* USER CODE END ETH_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x00808CD2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * @brief USART6 Initialization Function — PN532 HSU on Arduino D0/D1
  */
static void MX_USART6_UART_Init(void)
{
  huart6.Instance = USART6;
  huart6.Init.BaudRate = 115200;
  huart6.Init.WordLength = UART_WORDLENGTH_8B;
  huart6.Init.StopBits = UART_STOPBITS_1;
  huart6.Init.Parity = UART_PARITY_NONE;
  huart6.Init.Mode = UART_MODE_TX_RX;
  huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart6.Init.OverSampling = UART_OVERSAMPLING_16;
  huart6.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart6.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart6) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USB_OTG_FS Initialization Function
  * @param None
  * @retval None
  */
static void MX_USB_OTG_FS_PCD_Init(void)
{

  /* USER CODE BEGIN USB_OTG_FS_Init 0 */

  /* USER CODE END USB_OTG_FS_Init 0 */

  /* USER CODE BEGIN USB_OTG_FS_Init 1 */

  /* USER CODE END USB_OTG_FS_Init 1 */
  hpcd_USB_OTG_FS.Instance = USB_OTG_FS;
  hpcd_USB_OTG_FS.Init.dev_endpoints = 6;
  hpcd_USB_OTG_FS.Init.speed = PCD_SPEED_FULL;
  hpcd_USB_OTG_FS.Init.dma_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
  hpcd_USB_OTG_FS.Init.Sof_enable = ENABLE;
  hpcd_USB_OTG_FS.Init.low_power_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.lpm_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.vbus_sensing_enable = ENABLE;
  hpcd_USB_OTG_FS.Init.use_dedicated_ep1 = DISABLE;
  if (HAL_PCD_Init(&hpcd_USB_OTG_FS) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USB_OTG_FS_Init 2 */

  /* USER CODE END USB_OTG_FS_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LD1_Pin|LD3_Pin|LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(USB_PowerSwitchOn_GPIO_Port, USB_PowerSwitchOn_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : USER_Btn_Pin */
  GPIO_InitStruct.Pin = USER_Btn_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(USER_Btn_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LD1_Pin LD3_Pin LD2_Pin */
  GPIO_InitStruct.Pin = LD1_Pin|LD3_Pin|LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : USB_PowerSwitchOn_Pin */
  GPIO_InitStruct.Pin = USB_PowerSwitchOn_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(USB_PowerSwitchOn_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : USB_OverCurrent_Pin */
  GPIO_InitStruct.Pin = USB_OverCurrent_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(USB_OverCurrent_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
