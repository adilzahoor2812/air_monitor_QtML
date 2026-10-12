/**
 * Headless 480×320 capture of operation_hmi (same widgets as the TFT / SDL sim).
 * Usage: ./dump_hmi [out.ppm]
 */
#include "lvgl.h"
#include "operation_hmi.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HOR_RES 480
#define VER_RES 320

static uint32_t g_tick_ms;

static uint32_t tick_get_cb(void)
{
  return g_tick_ms;
}

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
  LV_UNUSED(area);
  LV_UNUSED(px_map);
  lv_display_flush_ready(disp);
}

static int write_ppm(const char *path, const uint8_t *fb, uint32_t stride)
{
  FILE *f = fopen(path, "wb");
  if(f == NULL) {
    perror(path);
    return 1;
  }
  fprintf(f, "P6\n%d %d\n255\n", HOR_RES, VER_RES);
  for(int y = 0; y < VER_RES; y++) {
    const uint16_t *row = (const uint16_t *)(fb + (size_t)y * stride);
    for(int x = 0; x < HOR_RES; x++) {
      uint16_t p = row[x];
      uint8_t r = (uint8_t)(((p >> 11) & 0x1Fu) * 255u / 31u);
      uint8_t g = (uint8_t)(((p >> 5) & 0x3Fu) * 255u / 63u);
      uint8_t b = (uint8_t)((p & 0x1Fu) * 255u / 31u);
      fputc(r, f);
      fputc(g, f);
      fputc(b, f);
    }
  }
  fclose(f);
  return 0;
}

int main(int argc, char **argv)
{
  const char *out = (argc >= 2) ? argv[1] : "air_monitor_ui.ppm";
  const int spark[] = {
      52, 61, 74, 58, 80, 69, 88, 63, 77, 55, 82, 70,
      91, 66, 73, 57, 84, 71, 79, 60, 86, 68, 75, 72
  };

  lv_init();
  lv_tick_set_cb(tick_get_cb);

  uint32_t stride = lv_draw_buf_width_to_stride(HOR_RES, LV_COLOR_FORMAT_RGB565);
  size_t buf_sz = (size_t)stride * VER_RES;
  uint8_t *fb = (uint8_t *)calloc(1, buf_sz);
  if(fb == NULL) {
    fprintf(stderr, "alloc failed\n");
    return 1;
  }

  lv_display_t *disp = lv_display_create(HOR_RES, VER_RES);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(disp, fb, NULL, (uint32_t)buf_sz, LV_DISPLAY_RENDER_MODE_FULL);
  lv_display_set_flush_cb(disp, flush_cb);

  operation_hmi_create();
  operation_hmi_set_temp_c(24.1f);
  operation_hmi_set_hum_pct(48.0f);
  for(unsigned i = 0; i < sizeof(spark) / sizeof(spark[0]); i++) {
    operation_hmi_set_distance_cm(spark[i]);
  }
  operation_hmi_set_link_eth(true);
  operation_hmi_set_link_f756(true);
  operation_hmi_set_cpu_pct(18);
  operation_hmi_set_heap_free_pct(62);
  operation_hmi_log_event(OPERATION_HMI_EVT_OK, "Sensor OK");
  operation_hmi_set_status("Demo — connect F756 for live data");

  for(int i = 0; i < 40; i++) {
    lv_timer_handler();
    g_tick_ms += 25;
    lv_tick_inc(25);
  }

  if(write_ppm(out, fb, stride) != 0) {
    free(fb);
    return 1;
  }
  printf("wrote %s (%dx%d)\n", out, HOR_RES, VER_RES);
  free(fb);
  return 0;
}
