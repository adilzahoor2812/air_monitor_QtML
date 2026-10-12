/**
 * @file app_tft.c
 * @brief SPI4 ILI9488/ST7796 bring-up. Commands 8-bit, pixels 16- or 18-bit.
 */
#include "app_tft.h"
#include "main.h"

#define TFT_CS_PORT   GPIOE
#define TFT_CS_PIN    GPIO_PIN_11 /* D5 */
#define TFT_DC_PORT   GPIOF
#define TFT_DC_PIN    GPIO_PIN_13 /* D7 */
#define TFT_RST_PORT  GPIOF
#define TFT_RST_PIN   GPIO_PIN_14 /* D4 */
#define TFT_LED_PORT  GPIOE
#define TFT_LED_PIN   GPIO_PIN_9  /* D6 */

#define TFT_CS_LOW()  HAL_GPIO_WritePin(TFT_CS_PORT, TFT_CS_PIN, GPIO_PIN_RESET)
#define TFT_CS_HIGH() HAL_GPIO_WritePin(TFT_CS_PORT, TFT_CS_PIN, GPIO_PIN_SET)
#define TFT_DC_CMD()  HAL_GPIO_WritePin(TFT_DC_PORT, TFT_DC_PIN, GPIO_PIN_RESET)
#define TFT_DC_DATA() HAL_GPIO_WritePin(TFT_DC_PORT, TFT_DC_PIN, GPIO_PIN_SET)

static void spi_w8(uint8_t v)
{
  while ((SPI4->SR & SPI_SR_TXE) == 0U)
  {
  }
  *((__IO uint8_t *)&SPI4->DR) = v;
}

static void spi_wait_idle(void)
{
  while ((SPI4->SR & SPI_SR_FTLVL) != 0U)
  {
  }
  while ((SPI4->SR & SPI_SR_BSY) != 0U)
  {
  }
  while ((SPI4->SR & SPI_SR_FRLVL) != 0U)
  {
    (void)*((__IO uint8_t *)&SPI4->DR);
  }
  if ((SPI4->SR & SPI_SR_OVR) != 0U)
  {
    (void)SPI4->DR;
    (void)SPI4->SR;
  }
}

static void tft_cmd(uint8_t cmd)
{
  spi_wait_idle();
  TFT_DC_CMD();
  TFT_CS_LOW();
  spi_w8(cmd);
  spi_wait_idle();
}

static void tft_data8(uint8_t d)
{
  TFT_DC_DATA();
  spi_w8(d);
}

static void tft_end(void)
{
  spi_wait_idle();
  TFT_CS_HIGH();
}

static void tft_cmd1(uint8_t cmd, uint8_t d0)
{
  tft_cmd(cmd);
  tft_data8(d0);
  tft_end();
}

static void gpio_out(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState ini)
{
  GPIO_InitTypeDef g = {0};
  HAL_GPIO_WritePin(port, pin, ini);
  g.Pin = pin;
  g.Mode = GPIO_MODE_OUTPUT_PP;
  g.Pull = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(port, &g);
}

static void spi4_gpio_init(void)
{
  GPIO_InitTypeDef g = {0};

  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_SPI4_CLK_ENABLE();

  /* Backlight first so the panel lights even if SPI init is wrong. */
  gpio_out(TFT_LED_PORT, TFT_LED_PIN, GPIO_PIN_SET);
  gpio_out(TFT_CS_PORT, TFT_CS_PIN, GPIO_PIN_SET);
  gpio_out(TFT_DC_PORT, TFT_DC_PIN, GPIO_PIN_SET);
  gpio_out(TFT_RST_PORT, TFT_RST_PIN, GPIO_PIN_SET);

  g.Pin = GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14; /* SCK MISO MOSI */
  g.Mode = GPIO_MODE_AF_PP;
  g.Pull = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  g.Alternate = GPIO_AF5_SPI4;
  HAL_GPIO_Init(GPIOE, &g);

  /* SPI4: master, mode 0, software NSS, ~9 MHz (APB2 72 MHz / 8). */
  SPI4->CR1 = 0;
  SPI4->CR2 = 0;
  SPI4->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_BR_1;
  SPI4->CR2 = (uint32_t)SPI_CR2_FRXTH | (7UL << SPI_CR2_DS_Pos);
  SPI4->CR1 |= SPI_CR1_SPE;
}

static void hw_reset(void)
{
  TFT_CS_HIGH();
  HAL_GPIO_WritePin(TFT_RST_PORT, TFT_RST_PIN, GPIO_PIN_SET);
  HAL_Delay(10);
  HAL_GPIO_WritePin(TFT_RST_PORT, TFT_RST_PIN, GPIO_PIN_RESET);
  HAL_Delay(20);
  HAL_GPIO_WritePin(TFT_RST_PORT, TFT_RST_PIN, GPIO_PIN_SET);
  HAL_Delay(150);
}

static void set_window(int32_t x1, int32_t y1, int32_t x2, int32_t y2)
{
  tft_cmd(0x2AU);
  tft_data8((uint8_t)((uint32_t)x1 >> 8));
  tft_data8((uint8_t)x1);
  tft_data8((uint8_t)((uint32_t)x2 >> 8));
  tft_data8((uint8_t)x2);
  tft_end();

  tft_cmd(0x2BU);
  tft_data8((uint8_t)((uint32_t)y1 >> 8));
  tft_data8((uint8_t)y1);
  tft_data8((uint8_t)((uint32_t)y2 >> 8));
  tft_data8((uint8_t)y2);
  tft_end();

  tft_cmd(0x2CU);
  TFT_DC_DATA();
}

static void pix_rgb565(uint16_t c)
{
#if APP_TFT_COLOR18
  spi_w8((uint8_t)((c >> 8) & 0xF8U));
  spi_w8((uint8_t)((c >> 3) & 0xFCU));
  spi_w8((uint8_t)((c << 3) & 0xF8U));
#else
  spi_w8((uint8_t)(c >> 8));
  spi_w8((uint8_t)c);
#endif
}

void AppTft_Backlight(uint8_t on)
{
  HAL_GPIO_WritePin(TFT_LED_PORT, TFT_LED_PIN,
                    on != 0U ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void AppTft_Fill(uint16_t rgb565)
{
  uint32_t n = (uint32_t)APP_TFT_HOR_RES * (uint32_t)APP_TFT_VER_RES;
  set_window(0, 0, APP_TFT_HOR_RES - 1, APP_TFT_VER_RES - 1);
  while (n != 0U)
  {
    pix_rgb565(rgb565);
    n--;
  }
  tft_end();
}

void AppTft_BlitRGB565(int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                       const uint16_t *pix)
{
  int32_t w;
  int32_t h;
  int32_t y;
  const uint16_t *row;

  if ((x1 > x2) || (y1 > y2) || (pix == NULL))
  {
    return;
  }
  if (x1 < 0)
  {
    x1 = 0;
  }
  if (y1 < 0)
  {
    y1 = 0;
  }
  if (x2 >= APP_TFT_HOR_RES)
  {
    x2 = APP_TFT_HOR_RES - 1;
  }
  if (y2 >= APP_TFT_VER_RES)
  {
    y2 = APP_TFT_VER_RES - 1;
  }

  w = x2 - x1 + 1;
  h = y2 - y1 + 1;
  set_window(x1, y1, x2, y2);
  row = pix;
  for (y = 0; y < h; y++)
  {
    int32_t x;
    for (x = 0; x < w; x++)
    {
      pix_rgb565(row[x]);
    }
    row = (const uint16_t *)((const uint8_t *)row + (uint32_t)w * 2U);
  }
  tft_end();
}

void AppTft_ColorBars(void)
{
  AppTft_Fill(0xF800U); /* red */
  HAL_Delay(180);
  AppTft_Fill(0x07E0U); /* green */
  HAL_Delay(180);
  AppTft_Fill(0x001FU); /* blue */
  HAL_Delay(180);
  AppTft_Fill(0xE71CU); /* light gray, close to HMI bg */
}

static void panel_init(void)
{
  /* Software reset + sleep out — both ILI9488 and ST7796. */
  tft_cmd(0x01U);
  tft_end();
  HAL_Delay(120);

  tft_cmd(0x11U);
  tft_end();
  HAL_Delay(120);

  /* ILI9488 power (harmless on many ST7796 clones). */
  tft_cmd(0xF7U);
  tft_data8(0xA9U);
  tft_data8(0x51U);
  tft_data8(0x2CU);
  tft_data8(0x82U);
  tft_end();

  tft_cmd(0xC0U);
  tft_data8(0x11U);
  tft_data8(0x09U);
  tft_end();

  tft_cmd1(0xC1U, 0x41U);

  tft_cmd(0xC5U);
  tft_data8(0x00U);
  tft_data8(0x0AU);
  tft_data8(0x80U);
  tft_end();

  tft_cmd(0xB1U);
  tft_data8(0xB0U);
  tft_data8(0x11U);
  tft_end();

  tft_cmd1(0xB4U, 0x02U);

  tft_cmd(0xB6U);
  tft_data8(0x02U);
  tft_data8(0x22U);
  tft_end();

  tft_cmd1(0x36U, (uint8_t)APP_TFT_MADCTL);
#if APP_TFT_COLOR18
  tft_cmd1(0x3AU, 0x66U); /* 18-bit */
#else
  tft_cmd1(0x3AU, 0x55U); /* 16-bit */
#endif

  tft_cmd(0x21U); /* INVON — IPS panels */
  tft_end();

  tft_cmd(0x13U); /* NORON */
  tft_end();

  tft_cmd(0x29U); /* DISPON */
  tft_end();
  HAL_Delay(30);
}

void AppTft_Init(void)
{
  spi4_gpio_init();
  AppTft_Backlight(1U);
  hw_reset();
  panel_init();
}
