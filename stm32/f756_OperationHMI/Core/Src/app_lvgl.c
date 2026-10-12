/**
 * @file app_lvgl.c
 * @brief LVGL display on SPI TFT (partial RGB565 buffer).
 */
#include "app_lvgl.h"
#include "app_tft.h"
#include "main.h"

#include "lvgl.h"

#define LV_BUF_LINES 24

static uint16_t s_draw_buf[APP_TFT_HOR_RES * LV_BUF_LINES];

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
  AppTft_BlitRGB565(area->x1, area->y1, area->x2, area->y2,
                    (const uint16_t *)px_map);
  lv_display_flush_ready(disp);
}

void AppLvgl_Init(void)
{
  lv_display_t *disp;

  lv_init();
  lv_tick_set_cb(HAL_GetTick);

  disp = lv_display_create(APP_TFT_HOR_RES, APP_TFT_VER_RES);
  lv_display_set_flush_cb(disp, flush_cb);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(disp, s_draw_buf, NULL, sizeof(s_draw_buf),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
}

void AppLvgl_Handler(void)
{
  (void)lv_timer_handler();
}
