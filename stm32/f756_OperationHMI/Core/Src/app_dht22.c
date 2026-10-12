/**
 * @file app_dht22.c
 * @brief DHT22 bit-bang driver — NUCLEO-F756ZG Arduino D2 = PF15.
 */
#include "app_dht22.h"
#include "main.h"

#define DHT_PORT GPIOF
#define DHT_PIN  GPIO_PIN_15

/* Cortex-M7 DWT lock access (required on some F7 parts). */
#define DWT_LAR (*((volatile uint32_t *)0xE0001FB0u))

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
  if (cycles == 0U)
  {
    cycles = 1U;
  }

  if ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) != 0U)
  {
    const uint32_t start = DWT->CYCCNT;
    while ((DWT->CYCCNT - start) < cycles)
    {
    }
  }
  else
  {
    /* Fallback if DWT is locked/unavailable. */
    while (cycles--)
    {
      __NOP();
    }
  }
}

static void pin_output(void)
{
  GPIO_InitTypeDef gpio = {0};
  gpio.Pin = DHT_PIN;
  gpio.Mode = GPIO_MODE_OUTPUT_OD;
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(DHT_PORT, &gpio);
}

static void pin_input(void)
{
  GPIO_InitTypeDef gpio = {0};
  gpio.Pin = DHT_PIN;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(DHT_PORT, &gpio);
}

static int wait_pin(GPIO_PinState want, uint32_t timeout_us)
{
  const uint32_t t0 = DWT->CYCCNT;
  const uint32_t limit = timeout_us * (SystemCoreClock / 1000000U);

  while (HAL_GPIO_ReadPin(DHT_PORT, DHT_PIN) != want)
  {
    if ((DWT->CYCCNT - t0) > limit)
    {
      return -1;
    }
  }
  return 0;
}

void AppDht22_Init(void)
{
  __HAL_RCC_GPIOF_CLK_ENABLE();
  dwt_init();
  pin_input();
  /* Sensor needs >1 s after power-up; keep this short so serial stays responsive. */
  HAL_Delay(1500);
}

int AppDht22_Read(AppDht22Sample_t *out)
{
  uint8_t data[5] = {0};
  uint8_t i;
  uint8_t j;

  if (out == 0)
  {
    return -1;
  }
  out->valid = 0;
  out->temperature_c = 0.0f;
  out->humidity_pct = 0.0f;

  /* Host start: >18 ms low */
  pin_output();
  HAL_GPIO_WritePin(DHT_PORT, DHT_PIN, GPIO_PIN_RESET);
  HAL_Delay(20);
  HAL_GPIO_WritePin(DHT_PORT, DHT_PIN, GPIO_PIN_SET);
  delay_us(30);
  pin_input();

  /* DHT response */
  if (wait_pin(GPIO_PIN_RESET, 200) != 0)
  {
    return -2; /* no response — wiring / wrong pin */
  }
  if (wait_pin(GPIO_PIN_SET, 200) != 0)
  {
    return -3;
  }
  if (wait_pin(GPIO_PIN_RESET, 200) != 0)
  {
    return -4;
  }

  for (i = 0; i < 5U; i++)
  {
    for (j = 0; j < 8U; j++)
    {
      if (wait_pin(GPIO_PIN_SET, 100) != 0)
      {
        return -5;
      }
      delay_us(40); /* sample mid-bit: ~28 us => 0, ~70 us => 1 */
      data[i] <<= 1;
      if (HAL_GPIO_ReadPin(DHT_PORT, DHT_PIN) == GPIO_PIN_SET)
      {
        data[i] |= 1U;
      }
      if (wait_pin(GPIO_PIN_RESET, 100) != 0)
      {
        /* last bit may already be low; tolerate */
        if (j != 7U || i != 4U)
        {
          return -6;
        }
      }
    }
  }

  if ((uint8_t)(data[0] + data[1] + data[2] + data[3]) != data[4])
  {
    return -7; /* checksum */
  }

  {
    const uint16_t hum = ((uint16_t)data[0] << 8) | data[1];
    const uint16_t raw_t = ((uint16_t)data[2] << 8) | data[3];
    int16_t temp;

    if ((raw_t & 0x8000U) != 0U)
    {
      temp = (int16_t)(-(int16_t)(raw_t & 0x7FFFU));
    }
    else
    {
      temp = (int16_t)raw_t;
    }

    out->humidity_pct = (float)hum / 10.0f;
    out->temperature_c = (float)temp / 10.0f;
    out->valid = 1U;
  }

  return 0;
}
