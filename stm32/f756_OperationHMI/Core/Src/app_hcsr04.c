/**
 * @file app_hcsr04.c
 * @brief HC-SR04 (GPIO + DWT µs timing).
 *
 * Wiring (NUCLEO-F756ZG Arduino headers):
 *   TRIG → D8  / PF12
 *   ECHO → D9  / PD15
 *   VCC  → 5V
 *   GND  → GND
 *
 * Boot auto-scan also accepts swapped D8/D9.
 */
#include "app_hcsr04.h"
#include "main.h"

#include <stdio.h>

#define TRIG_PULSE_US   12U
#define US_PER_CM       58U
#define MIN_ECHO_US     100U
#define MAX_ECHO_US     25000U
#define ECHO_TIMEOUT_US 30000U

#define DWT_LAR (*((volatile uint32_t *)0xE0001FB0u))

typedef struct
{
  GPIO_TypeDef *port;
  uint16_t pin;
  const char *name;
} AppGpioPin_t;

static const AppGpioPin_t k_d8 = {GPIOF, GPIO_PIN_12, "D8"};
static const AppGpioPin_t k_d9 = {GPIOD, GPIO_PIN_15, "D9"};

static const AppGpioPin_t *s_trig = &k_d8;
static const AppGpioPin_t *s_echo = &k_d9;
static int s_last_err;
static void (*s_log)(const char *msg);

void AppHcsr04_SetLog(void (*fn)(const char *msg))
{
  s_log = fn;
}

int AppHcsr04_LastError(void)
{
  return s_last_err;
}

static void log_msg(const char *m)
{
  if (s_log != NULL)
  {
    s_log(m);
  }
}

static void dwt_init(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT_LAR = 0xC5ACCE55u;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void delay_us(uint32_t us)
{
  uint32_t cycles = us * (SystemCoreClock / 1000000U);
  const uint32_t start = DWT->CYCCNT;
  if (cycles < 1U)
  {
    cycles = 1U;
  }
  while ((DWT->CYCCNT - start) < cycles)
  {
  }
}

static void cfg_out(const AppGpioPin_t *p)
{
  GPIO_InitTypeDef g = {0};
  if (p->port == GPIOF)
  {
    __HAL_RCC_GPIOF_CLK_ENABLE();
  }
  else
  {
    __HAL_RCC_GPIOD_CLK_ENABLE();
  }
  HAL_GPIO_DeInit(p->port, p->pin);
  HAL_GPIO_WritePin(p->port, p->pin, GPIO_PIN_RESET);
  g.Pin = p->pin;
  g.Mode = GPIO_MODE_OUTPUT_PP;
  g.Pull = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(p->port, &g);
}

static void cfg_in(const AppGpioPin_t *p)
{
  GPIO_InitTypeDef g = {0};
  if (p->port == GPIOF)
  {
    __HAL_RCC_GPIOF_CLK_ENABLE();
  }
  else
  {
    __HAL_RCC_GPIOD_CLK_ENABLE();
  }
  HAL_GPIO_DeInit(p->port, p->pin);
  g.Pin = p->pin;
  g.Mode = GPIO_MODE_INPUT;
  g.Pull = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(p->port, &g);
}

static int pin_high(const AppGpioPin_t *p)
{
  return (HAL_GPIO_ReadPin(p->port, p->pin) != GPIO_PIN_RESET) ? 1 : 0;
}

static int wait_echo(int want_high, uint32_t timeout_us)
{
  const uint32_t t0 = DWT->CYCCNT;
  const uint32_t limit = timeout_us * (SystemCoreClock / 1000000U);
  const int want = want_high ? 1 : 0;

  while (pin_high(s_echo) != want)
  {
    if ((DWT->CYCCNT - t0) > limit)
    {
      return -1;
    }
  }
  return 0;
}

static int try_pair(const AppGpioPin_t *trig, const AppGpioPin_t *echo, uint32_t *echo_us_out)
{
  uint32_t t_rise;
  uint32_t t_fall;
  uint32_t us;

  cfg_out(trig);
  cfg_in(echo);
  delay_us(500U);

  if (pin_high(echo))
  {
    return -2;
  }

  HAL_GPIO_WritePin(trig->port, trig->pin, GPIO_PIN_RESET);
  delay_us(2U);
  HAL_GPIO_WritePin(trig->port, trig->pin, GPIO_PIN_SET);
  delay_us(TRIG_PULSE_US);
  HAL_GPIO_WritePin(trig->port, trig->pin, GPIO_PIN_RESET);

  {
    const uint32_t t0 = DWT->CYCCNT;
    const uint32_t limit = ECHO_TIMEOUT_US * (SystemCoreClock / 1000000U);
    while (!pin_high(echo))
    {
      if ((DWT->CYCCNT - t0) > limit)
      {
        return -3;
      }
    }
  }
  t_rise = DWT->CYCCNT;

  {
    const uint32_t t0 = DWT->CYCCNT;
    const uint32_t limit = ECHO_TIMEOUT_US * (SystemCoreClock / 1000000U);
    while (pin_high(echo))
    {
      if ((DWT->CYCCNT - t0) > limit)
      {
        return -4;
      }
    }
  }
  t_fall = DWT->CYCCNT;

  us = (t_fall - t_rise) / (SystemCoreClock / 1000000U);
  if ((us < MIN_ECHO_US) || (us > MAX_ECHO_US))
  {
    return -5;
  }
  if (echo_us_out != NULL)
  {
    *echo_us_out = us;
  }
  return 0;
}

void AppHcsr04_Init(void)
{
  char line[96];
  uint32_t us = 0U;
  int rc;

  dwt_init();
  s_last_err = 0;
  s_trig = &k_d8;
  s_echo = &k_d9;

  (void)snprintf(line, sizeof(line), "SONAR clk=%lu MHz TRIG=D8 ECHO=D9\r\n",
                 (unsigned long)(SystemCoreClock / 1000000U));
  log_msg(line);

  rc = try_pair(&k_d8, &k_d9, &us);
  (void)snprintf(line, sizeof(line), "SONAR try TRIG=D8 ECHO=D9 -> %d\r\n", rc);
  log_msg(line);

  if (rc != 0)
  {
    HAL_Delay(70);
    rc = try_pair(&k_d9, &k_d8, &us);
    (void)snprintf(line, sizeof(line), "SONAR try TRIG=D9 ECHO=D8 -> %d\r\n", rc);
    log_msg(line);
    if (rc == 0)
    {
      s_trig = &k_d9;
      s_echo = &k_d8;
    }
  }

  if (rc == 0)
  {
    cfg_out(s_trig);
    cfg_in(s_echo);
    (void)snprintf(line, sizeof(line),
                   "SONAR LOCKED TRIG=%s ECHO=%s ~%lu cm\r\n",
                   s_trig->name, s_echo->name, (unsigned long)(us / US_PER_CM));
    log_msg(line);
    s_last_err = 0;
  }
  else
  {
    cfg_out(s_trig);
    cfg_in(s_echo);
    log_msg("SONAR NO ECHO — VCC=5V GND TRIG=D8 ECHO=D9\r\n");
    s_last_err = -3;
  }
}

int AppHcsr04_Read(AppHcsr04Sample_t *out)
{
  uint32_t t_rise;
  uint32_t t_fall;
  uint32_t us;
  uint32_t cm;

  if (out == NULL)
  {
    s_last_err = -1;
    return -1;
  }

  out->valid = 0U;
  out->distance_cm = 0U;
  out->echo_us = 0U;
  out->timestamp_ms = HAL_GetTick();

  /* Re-assert pin modes in case another driver touched GPIOs */
  cfg_out(s_trig);
  cfg_in(s_echo);

  if (wait_echo(0, 8000U) != 0)
  {
    s_last_err = -2;
    return -2;
  }

  HAL_GPIO_WritePin(s_trig->port, s_trig->pin, GPIO_PIN_RESET);
  delay_us(2U);
  HAL_GPIO_WritePin(s_trig->port, s_trig->pin, GPIO_PIN_SET);
  delay_us(TRIG_PULSE_US);
  HAL_GPIO_WritePin(s_trig->port, s_trig->pin, GPIO_PIN_RESET);

  if (wait_echo(1, ECHO_TIMEOUT_US) != 0)
  {
    s_last_err = -3;
    return -3;
  }
  t_rise = DWT->CYCCNT;

  if (wait_echo(0, ECHO_TIMEOUT_US) != 0)
  {
    s_last_err = -4;
    return -4;
  }
  t_fall = DWT->CYCCNT;

  us = (t_fall - t_rise) / (SystemCoreClock / 1000000U);
  out->echo_us = us;

  if ((us < MIN_ECHO_US) || (us > MAX_ECHO_US))
  {
    s_last_err = -5;
    return -5;
  }

  cm = us / US_PER_CM;
  if (cm == 0U)
  {
    cm = 1U;
  }
  out->distance_cm = cm;
  out->valid = 1U;
  s_last_err = 0;
  return 0;
}
