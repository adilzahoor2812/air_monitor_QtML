/**
 * @file app_tft.h
 * @brief SPI4 TFT — LCDWiki-style 480×320 (ILI9488 / ST7796).
 *
 * Wiring (NUCLEO-F756ZG), do not use D10–D13 (NFC):
 *   VCC→CN8 5V  GND→GND
 *   CS→D5/PE11  RESET→D4/PF14  DC→D7/PF13
 *   MOSI→D38/PE14  SCK→D39/PE12  MISO→D3/PE13 (optional)
 *   LED→D6/PE9 (firmware drives high) or tie LED to 3V3
 */
#ifndef APP_TFT_H
#define APP_TFT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define APP_TFT_HOR_RES 480
#define APP_TFT_VER_RES 320

/** 1 = RGB666 on SPI (ILI9488). 0 = RGB565 (many ST7796). */
#ifndef APP_TFT_COLOR18
#define APP_TFT_COLOR18 1
#endif

/** MADCTL: 0x28 landscape BGR. Try 0xE8 / 0x48 if rotated or mirrored. */
#ifndef APP_TFT_MADCTL
#define APP_TFT_MADCTL 0x28
#endif

void AppTft_Init(void);
void AppTft_Backlight(uint8_t on);
void AppTft_Fill(uint16_t rgb565);
void AppTft_ColorBars(void);
void AppTft_BlitRGB565(int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                       const uint16_t *pix);

#ifdef __cplusplus
}
#endif

#endif /* APP_TFT_H */
