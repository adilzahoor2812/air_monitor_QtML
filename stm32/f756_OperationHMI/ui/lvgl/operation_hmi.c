/**
 * @file operation_hmi.c
 * @brief Air Monitor HMI — LVGL 9, 480×320 German industrial Betriebsübersicht.
 */
#include "operation_hmi.h"

#include "lvgl.h"
#include "roboto_fonts.h"
#include "latin1_de.h"

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#define TREND_POINTS   24
#define DIST_BAR_MAX   100
#define TEMP_BAR_MIN   (-20)
#define TEMP_BAR_MAX   80
#define ACK_SESSION_S  30
#define EVT_CAP        12
#define EVT_TEXT       64
#define EVT_VIEW       4

typedef struct {
  uint32_t bg;
  uint32_t panel;
  uint32_t header;
  uint32_t footer;
  uint32_t line;
  uint32_t title;
  uint32_t muted;
  uint32_t value;
  uint32_t bar;
  uint32_t safe;
  uint32_t warn;
  uint32_t alarm;
} theme_palette_t;

static const theme_palette_t PALETTE = {
  .bg = 0xE8E8E8,
  .panel = 0xFFFFFF,
  .header = 0x1A4A8C,
  .footer = 0xD8D8D8,
  .line = 0xC8C8C8,
  .title = 0x111111,
  .muted = 0x555555,
  .value = 0x000000,
  .bar = 0x2F6DB3,
  .safe = 0x3D9B47,
  .warn = 0xD48912,
  .alarm = 0xC42B28,
};

static theme_palette_t T = PALETTE;
static bool ack_armed_state = false;
static char zone_kind = 'S';
static uint32_t zone_color = 0x3D9B47;

static lv_obj_t *scr_root;
static lv_obj_t *hdr_bar;
static lv_obj_t *lbl_h1;
static lv_obj_t *lbl_h2;
static lv_obj_t *lbl_clock;
static lv_obj_t *alarm_led;
static lv_obj_t *lbl_betrieb;

static lv_obj_t *box_ready;
static lv_obj_t *lbl_ready;
static lv_obj_t *lbl_stoerung;
static lv_obj_t *lbl_stoerung_val;
static lv_obj_t *btn_ack;
static lv_obj_t *lbl_ack_btn;
static lv_obj_t *lbl_ack_status;
static lv_obj_t *lbl_ack_timer;

static lv_obj_t *lbl_dist;
static lv_obj_t *lbl_dist_unit;
static lv_obj_t *bar_dist;
static lv_obj_t *chart_dist;
static lv_chart_series_t *ser_dist;

static lv_obj_t *lbl_temp;
static lv_obj_t *lbl_temp_unit;
static lv_obj_t *bar_temp;

static lv_obj_t *lbl_hum;
static lv_obj_t *lbl_hum_unit;
static lv_obj_t *bar_hum;

static lv_obj_t *lbl_sys_eth;
static lv_obj_t *lbl_sys_f756;
static lv_obj_t *lbl_sys_wd;
static lv_obj_t *lbl_sys_cpu;
static lv_obj_t *lbl_sys_heap;
static lv_obj_t *evt_lines[EVT_VIEW];

static lv_obj_t *page_home;
static lv_obj_t *page_cal;
static lv_obj_t *page_alarm;
static lv_obj_t *nav_bar;
static lv_obj_t *fkey[4];
static int section_idx;

static lv_obj_t *evt_overlay;
static lv_obj_t *evt_list;
static lv_obj_t *ack_dlg;
static lv_timer_t *ack_dlg_timer;
static lv_timer_t *clock_timer;
static lv_timer_t *ack_session_timer;
static lv_timer_t *ack_clear_timer;
static int32_t ack_remain_s;
static operation_hmi_ack_cb_t ack_cb;

static lv_obj_t *ttl_anlage;
static lv_obj_t *ttl_mess;
static lv_obj_t *ttl_sys;
static lv_obj_t *ttl_ereig;
static lv_obj_t *ttl_cal;
static lv_obj_t *ttl_alarm;
static lv_obj_t *lbl_name_dist;
static lv_obj_t *lbl_name_temp;
static lv_obj_t *lbl_name_hum;
static lv_obj_t *lbl_sys_eth_k;
static lv_obj_t *lbl_sys_heap_k;
static lv_obj_t *lbl_fkey[4];
static lv_obj_t *lbl_cal[6];
static lv_obj_t *btn_lang_de;
static lv_obj_t *btn_lang_en;
static bool lang_en;
static lv_obj_t *lbl_zone;
static lv_obj_t *lbl_status;
static lv_obj_t *lbl_alarm_pri;
static lv_obj_t *lbl_alarm_txt;

typedef struct {
  operation_hmi_evt_level_t level;
  char text[EVT_TEXT];
  uint32_t tick_ms;
} hmi_evt_t;

static hmi_evt_t evt_ring[EVT_CAP];
static uint8_t evt_head;
static uint8_t evt_count;
static char last_zone_logged[16];
static char last_ack_logged[64];
static bool links_inited;
static float last_temp_c;
static float last_hum_pct;
static int last_dist_cm = -1;
static int last_cpu_pct;
static int last_heap_free_pct = 100;
static bool has_sysmon;
static bool has_temp;
static bool has_hum;
static bool link_f756;
static bool link_f401;
static bool link_eth;
static bool link_nfc;

typedef enum {
  ALM_NONE = 0,
  ALM_TEMP,
  ALM_HUM,
  ALM_DIST,
  ALM_ZONE
} alarm_src_t;

static alarm_src_t alarm_src;
static int alarm_sev_latched;
static bool alarm_acked;
static char operator_id[24];

static void apply_ack_armed_style(void);
static void evt_refresh_strip(void);
static void evt_refresh_overlay_list(void);
static void evt_close_overlay(void);
static void on_evt_open(lv_event_t *e);
static void on_evt_close(lv_event_t *e);
static void show_section(int idx);
static void ack_dlg_close(void);
static void ack_dlg_show(bool ok, const char *title, const char *message);
static void update_ack_session_ui(void);
static void update_alarm_banner(void);
static void clock_timer_cb(lv_timer_t *timer);
static void ack_session_timer_cb(lv_timer_t *timer);
static void on_ack(lv_event_t *e);
static void update_system_row(lv_obj_t *val, bool ok, const char *ok_txt, const char *bad_txt);
static void apply_lang(void);
static void on_lang_de(lv_event_t *e);
static void on_lang_en(lv_event_t *e);
static const char *tr(const char *de, const char *en);

static lv_font_t font12;
static lv_font_t font14;
static lv_font_t font16;
static lv_font_t font20;
static lv_font_t font28;

static void fonts_init(void)
{
  font12 = roboto_regular_12;
  font14 = roboto_regular_14;
  font16 = roboto_medium_16;
  font20 = roboto_medium_20;
  font28 = roboto_medium_28;
  font12.fallback = &latin1_de_12;
  font14.fallback = &latin1_de_14;
  font16.fallback = &latin1_de_16;
  font28.fallback = &latin1_de_28;
}

static const lv_font_t *font_small(void) { return &font12; }
static const lv_font_t *font_body(void)  { return &font14; }
static const lv_font_t *font_mid(void)   { return &font16; }
static const lv_font_t *font_val(void)   { return &font20; }
static const lv_font_t *font_big(void)   { return &font28; }

static const char *tr(const char *de, const char *en)
{
  return lang_en ? en : de;
}

static void style_plain(lv_obj_t *obj)
{
  lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(obj, 0, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
  lv_obj_set_style_radius(obj, 0, 0);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

static void style_quiet_btn(lv_obj_t *btn)
{
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_style_outline_width(btn, 0, 0);
  lv_obj_set_style_outline_width(btn, 0, LV_STATE_FOCUSED);
  lv_obj_set_style_outline_width(btn, 0, LV_STATE_FOCUS_KEY);
}

static lv_obj_t *make_col_card(lv_obj_t *parent, const char *title, int32_t width,
                                lv_obj_t **out_title)
{
  lv_obj_t *card = lv_obj_create(parent);
  lv_obj_set_size(card, width, LV_PCT(100));
  lv_obj_set_style_bg_color(card, lv_color_hex(T.panel), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(card, lv_color_hex(T.line), 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_radius(card, 0, 0);
  lv_obj_set_style_pad_all(card, 0, 0);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *hdr = lv_obj_create(card);
  lv_obj_set_size(hdr, LV_PCT(100), 22);
  lv_obj_set_style_bg_color(hdr, lv_color_hex(T.header), 0);
  lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(hdr, 0, 0);
  lv_obj_set_style_radius(hdr, 0, 0);
  lv_obj_set_style_pad_all(hdr, 0, 0);
  lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *tl = lv_label_create(hdr);
  lv_label_set_text(tl, title);
  lv_obj_set_style_text_color(tl, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(tl, font_small(), 0);
  lv_obj_set_style_text_letter_space(tl, 1, 0);
  lv_obj_center(tl);
  if(out_title != NULL) {
    *out_title = tl;
  }
  return card;
}

static lv_obj_t *make_sys_row(lv_obj_t *parent, const char *key, lv_obj_t **out_val,
                              const char *initial, lv_obj_t **out_key)
{
  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_set_size(row, LV_PCT(100), 16);
  lv_obj_set_style_min_height(row, 16, 0);
  lv_obj_set_style_max_height(row, 16, 0);
  style_plain(row);
  lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(row, 1, 0);
  lv_obj_set_style_border_color(row, lv_color_hex(T.line), 0);
  lv_obj_set_style_pad_hor(row, 6, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t *k = lv_label_create(row);
  lv_label_set_text(k, key);
  lv_obj_set_style_text_color(k, lv_color_hex(T.title), 0);
  lv_obj_set_style_text_font(k, font_small(), 0);

  lv_obj_t *v = lv_label_create(row);
  lv_label_set_text(v, initial);
  lv_obj_set_style_text_color(v, lv_color_hex(T.safe), 0);
  lv_obj_set_style_text_font(v, font_small(), 0);
  *out_val = v;
  if(out_key != NULL) {
    *out_key = k;
  }
  return row;
}

static void scale_layout_cb(lv_event_t *e)
{
  lv_obj_t *row = lv_event_get_target(e);
  int32_t w = lv_obj_get_content_width(row);
  uint32_t n = lv_obj_get_child_count(row);
  for(uint32_t i = 0; i < n; i++) {
    lv_obj_t *c = lv_obj_get_child(row, i);
    int pct = (int)(intptr_t)lv_obj_get_user_data(c);
    int32_t lw = lv_obj_get_width(c);
    int32_t x = (w * pct) / 100 - lw / 2;
    if(x < 0) {
      x = 0;
    }
    if(x + lw > w) {
      x = w - lw;
    }
    lv_obj_set_x(c, x);
  }
}

static void add_scale_mark(lv_obj_t *row, const char *txt, int pct)
{
  lv_obj_t *l = lv_label_create(row);
  lv_label_set_text(l, txt);
  lv_obj_set_style_text_color(l, lv_color_hex(T.muted), 0);
  lv_obj_set_style_text_font(l, font_small(), 0);
  lv_obj_set_user_data(l, (void *)(intptr_t)pct);
}

static void make_scale(lv_obj_t *parent, const char *a, int pa, const char *b, int pb,
                       const char *c, int pc, const char *d, int pd)
{
  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_set_size(row, LV_PCT(100), 12);
  lv_obj_set_style_min_height(row, 12, 0);
  lv_obj_set_style_max_height(row, 12, 0);
  style_plain(row);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);
  add_scale_mark(row, a, pa);
  add_scale_mark(row, b, pb);
  add_scale_mark(row, c, pc);
  if(d != NULL) {
    add_scale_mark(row, d, pd);
  }
  lv_obj_add_event_cb(row, scale_layout_cb, LV_EVENT_SIZE_CHANGED, NULL);
  lv_obj_add_event_cb(row, scale_layout_cb, LV_EVENT_STYLE_CHANGED, NULL);
}

static lv_obj_t *make_bar(lv_obj_t *parent, int32_t bar_min, int32_t bar_max)
{
  lv_obj_t *bar = lv_bar_create(parent);
  lv_obj_set_size(bar, LV_PCT(100), 10);
  lv_bar_set_range(bar, bar_min, bar_max);
  lv_bar_set_value(bar, bar_min, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(bar, lv_color_hex(0xD0D0D0), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(bar, 2, LV_PART_MAIN);
  lv_obj_set_style_bg_color(bar, lv_color_hex(T.bar), LV_PART_INDICATOR);
  lv_obj_set_style_radius(bar, 2, LV_PART_INDICATOR);
  return bar;
}

static lv_obj_t *make_metric(lv_obj_t *parent, const char *name, const char *unit,
                             lv_obj_t **out_name, lv_obj_t **out_val, lv_obj_t **out_unit,
                             lv_obj_t **out_bar, int32_t bar_min, int32_t bar_max, bool spark,
                             const char *s0, const char *s1, const char *s2)
{
  lv_obj_t *box = lv_obj_create(parent);
  lv_obj_set_width(box, LV_PCT(100));
  lv_obj_set_flex_grow(box, spark ? 3 : 2);
  style_plain(box);
  lv_obj_clear_flag(box, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(box, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_clip_corner(box, true, 0);
  lv_obj_set_style_radius(box, 1, 0);
  lv_obj_set_style_border_side(box, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(box, 1, 0);
  lv_obj_set_style_border_color(box, lv_color_hex(T.line), 0);
  lv_obj_set_style_pad_hor(box, 8, 0);
  lv_obj_set_style_pad_ver(box, 3, 0);
  lv_obj_set_style_pad_row(box, 2, 0);
  lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t *hdr = lv_obj_create(box);
  lv_obj_set_size(hdr, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_min_height(hdr, 24, 0);
  lv_obj_set_flex_grow(hdr, 0); /* never shrink value row for spark chart */
  style_plain(hdr);
  lv_obj_set_flex_flow(hdr, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(hdr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);

  lv_obj_t *nl = lv_label_create(hdr);
  lv_label_set_text(nl, name);
  lv_obj_set_style_text_color(nl, lv_color_hex(T.title), 0);
  lv_obj_set_style_text_font(nl, font_body(), 0);
  lv_obj_set_width(nl, 86);
  lv_label_set_long_mode(nl, LV_LABEL_LONG_CLIP);
  if(out_name != NULL) {
    *out_name = nl;
  }

  lv_obj_t *val = lv_label_create(hdr);
  lv_label_set_text(val, "--");
  lv_obj_set_style_text_color(val, lv_color_hex(T.value), 0);
  lv_obj_set_style_text_font(val, font_val(), 0);
  lv_obj_set_style_text_align(val, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_flex_grow(val, 1);
  *out_val = val;

  lv_obj_t *un = lv_label_create(hdr);
  lv_label_set_text(un, unit);
  lv_obj_set_style_text_color(un, lv_color_hex(T.title), 0);
  lv_obj_set_style_text_font(un, font_small(), 0);
  lv_obj_set_width(un, 28);
  lv_obj_set_style_text_align(un, LV_TEXT_ALIGN_RIGHT, 0);
  *out_unit = un;

  *out_bar = make_bar(box, bar_min, bar_max);
  if(bar_min < 0) {
    make_scale(box, s0, 0, s1, 20, "50", 70, s2, 100);
  } else {
    make_scale(box, s0, 0, s1, 50, s2, 100, NULL, 0);
  }

  if(spark) {
    lv_obj_t *ch = lv_chart_create(box);
    lv_obj_set_width(ch, LV_PCT(100));
    lv_obj_set_height(ch, 36); /* fixed — do not flex-crush Distanz value */
    lv_obj_set_flex_grow(ch, 0);
    lv_obj_clear_flag(ch, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(ch, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_clip_corner(ch, true, 0);
    lv_chart_set_type(ch, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(ch, TREND_POINTS);
    lv_chart_set_range(ch, LV_CHART_AXIS_PRIMARY_Y, 0, DIST_BAR_MAX);
    lv_chart_set_update_mode(ch, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_div_line_count(ch, 2, 0);
    lv_obj_set_style_bg_opa(ch, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ch, 1, 0);
    lv_obj_set_style_border_color(ch, lv_color_hex(T.line), 0);
    lv_obj_set_style_pad_all(ch, 2, 0);
    lv_obj_set_style_pad_left(ch, 2, 0);
    lv_obj_set_style_pad_right(ch, 2, 0);
    lv_obj_set_style_line_width(ch, 2, LV_PART_ITEMS);
    lv_obj_set_style_line_color(ch, lv_color_hex(T.bar), LV_PART_ITEMS);
    lv_obj_set_style_size(ch, 0, 0, LV_PART_INDICATOR);
    chart_dist = ch;
    ser_dist = lv_chart_add_series(ch, lv_color_hex(T.bar), LV_CHART_AXIS_PRIMARY_Y);
  }
  return box;
}

static int32_t clamp_i(int32_t v, int32_t lo, int32_t hi)
{
  if(v < lo) {
    return lo;
  }
  if(v > hi) {
    return hi;
  }
  return v;
}

static void anim_set_opa(void *obj, int32_t v)
{
  lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
}

static void pulse_dot_start(lv_obj_t *dot)
{
  if(dot == NULL) {
    return;
  }
  lv_anim_delete(dot, anim_set_opa);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, dot);
  lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_40);
  lv_anim_set_duration(&a, 700);
  lv_anim_set_playback_duration(&a, 700);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_exec_cb(&a, anim_set_opa);
  lv_anim_start(&a);
}

static void pulse_dot_stop(lv_obj_t *dot)
{
  if(dot == NULL) {
    return;
  }
  lv_anim_delete(dot, anim_set_opa);
  lv_obj_set_style_opa(dot, LV_OPA_COVER, 0);
}

static uint32_t zone_color_from_kind(char kind)
{
  if(kind == 'W') {
    return T.warn;
  }
  if(kind == 'A' || kind == 'F') {
    return T.alarm;
  }
  return T.safe;
}

static int sev_temp(float t)
{
  if(t < 0.0f || t > 40.0f) {
    return 2;
  }
  if(t < 5.0f || t > 32.0f) {
    return 1;
  }
  return 0;
}

static int sev_hum(float h)
{
  if(h < 15.0f || h > 90.0f) {
    return 2;
  }
  if(h < 25.0f || h > 75.0f) {
    return 1;
  }
  return 0;
}

static int sev_dist(int d)
{
  if(d < 0) {
    return 0;
  }
  if(d < 20) {
    return 2;
  }
  if(d < 50) {
    return 1;
  }
  return 0;
}

static void evaluate_alarm(alarm_src_t *src, int *sev, char *msg, size_t msg_sz)
{
  *src = ALM_NONE;
  *sev = 0;
  if(msg && msg_sz > 0) {
    snprintf(msg, msg_sz, "%s", tr("keine", "none"));
  }
  if(has_temp && sev_temp(last_temp_c) >= 2) {
    const int t10 = (int)(last_temp_c * 10.0f);
    const int t_abs = (t10 < 0) ? -t10 : t10;
    *src = ALM_TEMP;
    *sev = 2;
    snprintf(msg, msg_sz, tr("Temperatur %s%d.%d C", "Temp %s%d.%d C"),
             (t10 < 0) ? "-" : "", t_abs / 10, t_abs % 10);
  } else if(has_hum && sev_hum(last_hum_pct) >= 2) {
    const int h10 = (int)(last_hum_pct * 10.0f);
    *src = ALM_HUM;
    *sev = 2;
    snprintf(msg, msg_sz, tr("Feuchte %d.%d %%", "Humidity %d.%d %%"),
             h10 / 10, ((h10 < 0) ? -h10 : h10) % 10);
  } else if(sev_dist(last_dist_cm) >= 2) {
    *src = ALM_DIST;
    *sev = 2;
    snprintf(msg, msg_sz, tr("Distanz %d cm", "Distance %d cm"), last_dist_cm);
  } else if(zone_kind == 'A' || zone_kind == 'F') {
    *src = ALM_ZONE;
    *sev = 2;
    snprintf(msg, msg_sz, "Zone ALARM");
  } else if(has_temp && sev_temp(last_temp_c) >= 1) {
    const int t10 = (int)(last_temp_c * 10.0f);
    const int t_abs = (t10 < 0) ? -t10 : t10;
    *src = ALM_TEMP;
    *sev = 1;
    snprintf(msg, msg_sz, tr("Temperatur %s%d.%d C", "Temp %s%d.%d C"),
             (t10 < 0) ? "-" : "", t_abs / 10, t_abs % 10);
  } else if(has_hum && sev_hum(last_hum_pct) >= 1) {
    const int h10 = (int)(last_hum_pct * 10.0f);
    *src = ALM_HUM;
    *sev = 1;
    snprintf(msg, msg_sz, tr("Feuchte %d.%d %%", "Humidity %d.%d %%"),
             h10 / 10, ((h10 < 0) ? -h10 : h10) % 10);
  } else if(sev_dist(last_dist_cm) >= 1) {
    *src = ALM_DIST;
    *sev = 1;
    snprintf(msg, msg_sz, tr("Distanz %d cm", "Distance %d cm"), last_dist_cm);
  } else if(zone_kind == 'W') {
    *src = ALM_ZONE;
    *sev = 1;
    snprintf(msg, msg_sz, "Zone WARN");
  }
}

static void update_alarm_banner(void)
{
  alarm_src_t src;
  int sev;
  char msg[56];
  evaluate_alarm(&src, &sev, msg, sizeof(msg));
  if(src != alarm_src || sev != alarm_sev_latched) {
    alarm_src = src;
    alarm_sev_latched = sev;
    alarm_acked = false;
  }
  if(sev == 0) {
    alarm_acked = false;
  }

  uint32_t box_c = T.safe;
  const char *ready = tr("BEREIT", "READY");
  const char *betrieb = tr("BETRIEB", "OPERATION");
  uint32_t led_c = T.safe;
  const char *stv = tr("keine", "none");
  uint32_t stc = T.safe;

  if(sev >= 2) {
    box_c = T.alarm;
    ready = alarm_acked ? tr("QUITT.", "ACK'd") : tr("STÖRUNG", "FAULT");
    betrieb = tr("STÖRUNG", "FAULT");
    led_c = T.alarm;
    stv = msg;
    stc = T.alarm;
  } else if(sev >= 1) {
    box_c = T.warn;
    ready = alarm_acked ? tr("QUITT.", "ACK'd") : tr("WARNUNG", "WARNING");
    betrieb = tr("WARNUNG", "WARNING");
    led_c = T.warn;
    stv = msg;
    stc = T.warn;
  }

  if(box_ready) {
    lv_obj_set_style_bg_color(box_ready, lv_color_hex(box_c), 0);
  }
  if(lbl_ready) {
    lv_label_set_text(lbl_ready, ready);
    lv_obj_set_style_text_font(lbl_ready, strlen(ready) > 6 ? font_val() : font_big(), 0);
  }
  if(lbl_stoerung_val) {
    lv_label_set_text(lbl_stoerung_val, stv);
    lv_obj_set_style_text_color(lbl_stoerung_val, lv_color_hex(stc), 0);
  }
  if(lbl_betrieb) {
    lv_label_set_text(lbl_betrieb, betrieb);
  }
  if(alarm_led) {
    lv_obj_set_style_bg_color(alarm_led, lv_color_hex(led_c), 0);
    if(sev >= 1 && !alarm_acked) {
      pulse_dot_start(alarm_led);
    } else {
      pulse_dot_stop(alarm_led);
    }
  }
  if(lbl_alarm_pri) {
    lv_label_set_text(lbl_alarm_pri, ready);
    lv_obj_set_style_text_color(lbl_alarm_pri, lv_color_hex(box_c), 0);
  }
  if(lbl_alarm_txt) {
    lv_label_set_text(lbl_alarm_txt, sev == 0 ? tr("Keine aktive Störung.", "No active fault.") : msg);
  }
  if(lbl_zone) {
    const char *z = "SAFE";
    if(zone_kind == 'W') {
      z = "WARN";
    } else if(zone_kind == 'A') {
      z = "ALARM";
    } else if(zone_kind == 'F') {
      z = "FAULT";
    }
    lv_label_set_text(lbl_zone, z);
    lv_obj_set_style_text_color(lbl_zone, lv_color_hex(zone_color), 0);
  }
}

static void update_ack_session_ui(void)
{
  if(lbl_ack_timer == NULL) {
    return;
  }
  if(ack_armed_state && ack_remain_s > 0) {
    char buf[24];
    snprintf(buf, sizeof(buf), tr("freigegeben %ds", "released %ds"), (int)ack_remain_s);
    lv_label_set_text(lbl_ack_timer, buf);
    lv_obj_set_style_text_color(lbl_ack_timer, lv_color_hex(T.header), 0);
  } else {
    lv_label_set_text(lbl_ack_timer, tr("Operator-Freigabe", "Operator Release"));
    lv_obj_set_style_text_color(lbl_ack_timer, lv_color_hex(T.muted), 0);
  }
}

static void apply_ack_armed_style(void)
{
  if(btn_ack == NULL) {
    return;
  }
  if(ack_armed_state) {
    lv_obj_set_style_bg_color(btn_ack, lv_color_hex(0xE8E8E8), 0);
    lv_obj_set_style_border_color(btn_ack, lv_color_hex(0x222222), 0);
    if(lbl_ack_btn) {
      lv_obj_set_style_text_color(lbl_ack_btn, lv_color_hex(T.title), 0);
    }
  } else {
    lv_obj_set_style_bg_color(btn_ack, lv_color_hex(0xEEEEEE), 0);
    lv_obj_set_style_border_color(btn_ack, lv_color_hex(0x888888), 0);
    if(lbl_ack_btn) {
      lv_obj_set_style_text_color(lbl_ack_btn, lv_color_hex(T.muted), 0);
    }
  }
}

static void update_system_row(lv_obj_t *val, bool ok, const char *ok_txt, const char *bad_txt)
{
  if(val == NULL) {
    return;
  }
  lv_label_set_text(val, ok ? ok_txt : bad_txt);
  lv_obj_set_style_text_color(val, lv_color_hex(ok ? T.safe : T.alarm), 0);
}

static void clock_timer_cb(lv_timer_t *timer)
{
  LV_UNUSED(timer);
  if(lbl_clock == NULL) {
    return;
  }
  time_t now = time(NULL);
  struct tm *lt = localtime(&now);
  char buf[16];
  if(lt == NULL) {
    lv_label_set_text(lbl_clock, "--:--:--");
    return;
  }
  strftime(buf, sizeof(buf), "%H:%M:%S", lt);
  lv_label_set_text(lbl_clock, buf);
}

static void ack_session_timer_cb(lv_timer_t *timer)
{
  LV_UNUSED(timer);
  if(!ack_armed_state) {
    return;
  }
  if(ack_remain_s > 0) {
    ack_remain_s--;
  }
  update_ack_session_ui();
}

static void on_ack(lv_event_t *e)
{
  LV_UNUSED(e);
  if(ack_cb) {
    ack_cb();
  } else {
    operation_hmi_confirm_ack(true, operator_id[0] ? operator_id : "LOCAL");
  }
}

void operation_hmi_set_ack_callback(operation_hmi_ack_cb_t cb)
{
  ack_cb = cb;
}

static void style_fkey(int idx, bool active)
{
  if(idx < 0 || idx > 3 || fkey[idx] == NULL) {
    return;
  }
  lv_obj_set_style_bg_color(fkey[idx], lv_color_hex(active ? 0xC5D4E8 : T.footer), 0);
  if(lbl_fkey[idx]) {
    lv_obj_set_style_text_color(lbl_fkey[idx], lv_color_hex(active ? T.header : T.title), 0);
  }
}

static void show_section(int idx)
{
  section_idx = idx;
  if(page_home) {
    if(idx == 0) {
      lv_obj_clear_flag(page_home, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(page_home, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if(page_cal) {
    if(idx == 1) {
      lv_obj_clear_flag(page_cal, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(page_cal, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if(page_alarm) {
    if(idx == 3) {
      lv_obj_clear_flag(page_alarm, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(page_alarm, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if(lbl_h2) {
    if(idx == 0) {
      lv_label_set_text(lbl_h2, tr("Betriebsübersicht", "Operation Overview"));
    } else if(idx == 1) {
      lv_label_set_text(lbl_h2, tr("Kalibrierung", "Calibration"));
    } else if(idx == 3) {
      lv_label_set_text(lbl_h2, "Alarm");
    }
  }
  for(int i = 0; i < 4; i++) {
    style_fkey(i, i == idx);
  }
}

static void on_f1(lv_event_t *e) { LV_UNUSED(e); show_section(0); }
static void on_f2(lv_event_t *e) { LV_UNUSED(e); show_section(1); }
static void on_f3(lv_event_t *e) { LV_UNUSED(e); on_evt_open(e); }
static void on_f4(lv_event_t *e) { LV_UNUSED(e); show_section(3); }

static lv_obj_t *make_fkey(lv_obj_t *parent, const char *name, lv_event_cb_t cb,
                            lv_obj_t **out_name)
{
  lv_obj_t *btn = lv_button_create(parent);
  lv_obj_set_flex_grow(btn, 1);
  lv_obj_set_height(btn, LV_PCT(100));
  lv_obj_set_style_bg_color(btn, lv_color_hex(T.footer), 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(btn, 0, 0);
  lv_obj_set_style_border_side(btn, LV_BORDER_SIDE_RIGHT, 0);
  lv_obj_set_style_border_width(btn, 1, 0);
  lv_obj_set_style_border_color(btn, lv_color_hex(0xB0B0B0), 0);
  lv_obj_set_style_pad_all(btn, 4, 0);
  lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  style_quiet_btn(btn);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *nl = lv_label_create(btn);
  lv_label_set_text(nl, name);
  lv_obj_set_style_text_color(nl, lv_color_hex(T.title), 0);
  lv_obj_set_style_text_font(nl, font_small(), 0);
  lv_label_set_long_mode(nl, LV_LABEL_LONG_CLIP);
  if(out_name != NULL) {
    *out_name = nl;
  }
  return btn;
}

static const char *evt_tr_text(const char *text)
{
  if(text == NULL) {
    return "";
  }
  if(strcmp(text, "Systemstart") == 0) {
    return tr("Systemstart", "System start");
  }
  if(strcmp(text, "Sensor OK") == 0) {
    return "Sensor OK";
  }
  if(strcmp(text, "Ethernet up") == 0) {
    return tr("Ethernet an", "Ethernet up");
  }
  if(strcmp(text, "Ethernet down") == 0) {
    return tr("Ethernet aus", "Ethernet down");
  }
  if(strcmp(text, "Freigabe aktiv") == 0) {
    return tr("Freigabe aktiv", "Release armed");
  }
  if(strcmp(text, "Freigabe gesperrt") == 0) {
    return tr("Freigabe gesperrt", "Release locked");
  }
  return text;
}

static uint32_t evt_level_color(operation_hmi_evt_level_t level)
{
  switch(level) {
    case OPERATION_HMI_EVT_OK:    return T.safe;
    case OPERATION_HMI_EVT_WARN:  return T.warn;
    case OPERATION_HMI_EVT_ALARM: return T.alarm;
    default:                      return T.header;
  }
}

static void evt_format_time(uint32_t tick_ms, char *out, size_t out_sz)
{
  time_t now = time(NULL);
  struct tm *lt = localtime(&now);
  if(lt) {
    strftime(out, out_sz, "%H:%M", lt);
    (void)tick_ms;
    return;
  }
  uint32_t sec = tick_ms / 1000u;
  snprintf(out, out_sz, "%02u:%02u", (unsigned)((sec / 60u) % 100u), (unsigned)(sec % 60u));
}

static void evt_refresh_strip(void)
{
  for(int i = 0; i < EVT_VIEW; i++) {
    if(evt_lines[i] == NULL) {
      continue;
    }
    if(i >= evt_count) {
      lv_label_set_text(evt_lines[i], "");
      continue;
    }
    int newest = (int)evt_head - 1 - i;
    while(newest < 0) {
      newest += EVT_CAP;
    }
    char tbuf[8];
    evt_format_time(evt_ring[newest].tick_ms, tbuf, sizeof(tbuf));
    char line[80];
    snprintf(line, sizeof(line), "%s  %s", tbuf, evt_tr_text(evt_ring[newest].text));
    lv_label_set_text(evt_lines[i], line);
    lv_obj_set_style_text_color(evt_lines[i], lv_color_hex(T.title), 0);
  }
}

static void evt_refresh_overlay_list(void)
{
  if(evt_list == NULL) {
    return;
  }
  lv_obj_clean(evt_list);
  if(evt_count == 0) {
    lv_obj_t *empty = lv_label_create(evt_list);
    lv_label_set_text(empty, tr("keine Ereignisse", "no events"));
    lv_obj_set_style_text_color(empty, lv_color_hex(T.muted), 0);
    lv_obj_set_style_text_font(empty, font_small(), 0);
    return;
  }
  for(uint8_t n = 0; n < evt_count; n++) {
    int idx = (int)evt_head - 1 - (int)n;
    while(idx < 0) {
      idx += EVT_CAP;
    }
    lv_obj_t *row = lv_obj_create(evt_list);
    lv_obj_set_size(row, LV_PCT(100), 22);
    style_plain(row);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, lv_color_hex(T.line), 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, 8, 0);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *dot = lv_obj_create(row);
    lv_obj_set_size(dot, 8, 8);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(evt_level_color(evt_ring[idx].level)), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dot, 0, 0);

    char tbuf[8];
    evt_format_time(evt_ring[idx].tick_ms, tbuf, sizeof(tbuf));
    lv_obj_t *tm = lv_label_create(row);
    lv_label_set_text(tm, tbuf);
    lv_obj_set_style_text_color(tm, lv_color_hex(T.muted), 0);
    lv_obj_set_style_text_font(tm, font_small(), 0);

    lv_obj_t *tx = lv_label_create(row);
    lv_label_set_text(tx, evt_tr_text(evt_ring[idx].text));
    lv_obj_set_style_text_color(tx, lv_color_hex(T.title), 0);
    lv_obj_set_style_text_font(tx, font_small(), 0);
  }
}

static void evt_close_overlay(void)
{
  if(evt_overlay) {
    lv_obj_delete(evt_overlay);
    evt_overlay = NULL;
    evt_list = NULL;
  }
}

static void on_evt_close(lv_event_t *e)
{
  LV_UNUSED(e);
  evt_close_overlay();
}

static void on_evt_open(lv_event_t *e)
{
  LV_UNUSED(e);
  if(scr_root == NULL) {
    return;
  }
  if(evt_overlay != NULL) {
    evt_close_overlay();
    return;
  }
  evt_overlay = lv_obj_create(scr_root);
  lv_obj_set_size(evt_overlay, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(evt_overlay, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(evt_overlay, LV_OPA_50, 0);
  lv_obj_set_style_border_width(evt_overlay, 0, 0);
  lv_obj_set_style_radius(evt_overlay, 0, 0);
  lv_obj_set_style_pad_all(evt_overlay, 16, 0);
  lv_obj_clear_flag(evt_overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(evt_overlay, on_evt_close, LV_EVENT_CLICKED, NULL);
  lv_obj_move_foreground(evt_overlay);

  lv_obj_t *panel = lv_obj_create(evt_overlay);
  lv_obj_set_size(panel, 420, 240);
  lv_obj_center(panel);
  lv_obj_set_style_bg_color(panel, lv_color_hex(T.panel), 0);
  lv_obj_set_style_border_color(panel, lv_color_hex(T.header), 0);
  lv_obj_set_style_border_width(panel, 2, 0);
  lv_obj_set_style_radius(panel, 0, 0);
  lv_obj_set_style_pad_all(panel, 0, 0);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_add_flag(panel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_EVENT_BUBBLE);

  lv_obj_t *hdr = lv_obj_create(panel);
  lv_obj_set_size(hdr, LV_PCT(100), 28);
  lv_obj_set_style_bg_color(hdr, lv_color_hex(T.header), 0);
  lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(hdr, 0, 0);
  lv_obj_set_style_radius(hdr, 0, 0);
  lv_obj_set_style_pad_hor(hdr, 10, 0);
  lv_obj_set_flex_flow(hdr, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(hdr, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t *ht = lv_label_create(hdr);
  lv_label_set_text(ht, tr("PROTOKOLL", "EVENT LOG"));
  lv_obj_set_style_text_color(ht, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(ht, font_body(), 0);

  lv_obj_t *xbtn = lv_button_create(hdr);
  lv_obj_set_size(xbtn, 28, 20);
  lv_obj_set_style_bg_color(xbtn, lv_color_hex(0x0E3A78), 0);
  lv_obj_set_style_radius(xbtn, 0, 0);
  style_quiet_btn(xbtn);
  lv_obj_add_event_cb(xbtn, on_evt_close, LV_EVENT_CLICKED, NULL);
  lv_obj_t *xl = lv_label_create(xbtn);
  lv_label_set_text(xl, "X");
  lv_obj_set_style_text_color(xl, lv_color_hex(0xFFFFFF), 0);
  lv_obj_center(xl);

  evt_list = lv_obj_create(panel);
  lv_obj_set_width(evt_list, LV_PCT(100));
  lv_obj_set_flex_grow(evt_list, 1);
  style_plain(evt_list);
  lv_obj_set_style_pad_all(evt_list, 8, 0);
  lv_obj_set_style_pad_row(evt_list, 2, 0);
  lv_obj_set_flex_flow(evt_list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(evt_list, LV_DIR_VER);
  evt_refresh_overlay_list();
}

static void ack_dlg_timer_cb(lv_timer_t *timer)
{
  LV_UNUSED(timer);
  ack_dlg_timer = NULL;
  ack_dlg_close();
}

static void on_ack_dlg_ok(lv_event_t *e)
{
  LV_UNUSED(e);
  ack_dlg_close();
}

static void ack_dlg_close(void)
{
  if(ack_dlg_timer != NULL) {
    lv_timer_delete(ack_dlg_timer);
    ack_dlg_timer = NULL;
  }
  if(ack_dlg != NULL) {
    lv_obj_delete(ack_dlg);
    ack_dlg = NULL;
  }
}

static void ack_dlg_show(bool ok, const char *title, const char *message)
{
  if(scr_root == NULL || title == NULL || message == NULL) {
    return;
  }
  ack_dlg_close();
  ack_dlg = lv_obj_create(scr_root);
  lv_obj_set_size(ack_dlg, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(ack_dlg, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(ack_dlg, LV_OPA_50, 0);
  lv_obj_set_style_border_width(ack_dlg, 0, 0);
  lv_obj_set_style_pad_all(ack_dlg, 0, 0);
  lv_obj_clear_flag(ack_dlg, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_move_foreground(ack_dlg);

  lv_obj_t *panel = lv_obj_create(ack_dlg);
  lv_obj_set_size(panel, 320, 160);
  lv_obj_center(panel);
  lv_obj_set_style_bg_color(panel, lv_color_hex(T.panel), 0);
  lv_obj_set_style_border_color(panel, lv_color_hex(ok ? T.safe : T.alarm), 0);
  lv_obj_set_style_border_width(panel, 2, 0);
  lv_obj_set_style_radius(panel, 0, 0);
  lv_obj_set_style_pad_all(panel, 14, 0);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t *ht = lv_label_create(panel);
  lv_label_set_text(ht, title);
  lv_obj_set_style_text_color(ht, lv_color_hex(ok ? T.safe : T.alarm), 0);
  lv_obj_set_style_text_font(ht, font_mid(), 0);

  lv_obj_t *msg = lv_label_create(panel);
  lv_label_set_text(msg, message);
  lv_obj_set_style_text_color(msg, lv_color_hex(T.title), 0);
  lv_obj_set_style_text_font(msg, font_small(), 0);
  lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(msg, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(msg, LV_PCT(100));

  lv_obj_t *ok_btn = lv_button_create(panel);
  lv_obj_set_size(ok_btn, 100, 28);
  lv_obj_set_style_bg_color(ok_btn, lv_color_hex(ok ? T.safe : T.alarm), 0);
  lv_obj_set_style_radius(ok_btn, 0, 0);
  style_quiet_btn(ok_btn);
  lv_obj_add_event_cb(ok_btn, on_ack_dlg_ok, LV_EVENT_CLICKED, NULL);
  lv_obj_t *ok_lbl = lv_label_create(ok_btn);
  lv_label_set_text(ok_lbl, "OK");
  lv_obj_set_style_text_color(ok_lbl, lv_color_hex(0xFFFFFF), 0);
  lv_obj_center(ok_lbl);

  ack_dlg_timer = lv_timer_create(ack_dlg_timer_cb, 10000, NULL);
  if(ack_dlg_timer != NULL) {
    lv_timer_set_repeat_count(ack_dlg_timer, 1);
  }
}

static void style_lang_btn(lv_obj_t *btn, bool active)
{
  if(btn == NULL) {
    return;
  }
  lv_obj_set_style_bg_color(btn, lv_color_hex(active ? T.header : 0x2A2A2A), 0);
}

static void apply_lang(void)
{
  style_lang_btn(btn_lang_de, !lang_en);
  style_lang_btn(btn_lang_en, lang_en);
  if(ttl_anlage) {
    lv_label_set_text(ttl_anlage, tr("ANLAGE", "PLANT"));
  }
  if(ttl_mess) {
    lv_label_set_text(ttl_mess, tr("MESSWERTE", "MEASURED VALUES"));
    lv_obj_set_style_text_letter_space(ttl_mess, lang_en ? 0 : 1, 0);
  }
  if(ttl_sys) {
    lv_label_set_text(ttl_sys, "SYSTEM");
  }
  if(ttl_ereig) {
    lv_label_set_text(ttl_ereig, tr("EREIGNISSE", "EVENTS"));
  }
  if(ttl_cal) {
    lv_label_set_text(ttl_cal, tr("KALIBRIERUNG", "CALIBRATION"));
  }
  if(ttl_alarm) {
    lv_label_set_text(ttl_alarm, "ALARM");
  }
  if(lbl_stoerung) {
    lv_label_set_text(lbl_stoerung, tr("Störung:", "Fault:"));
  }
  if(lbl_ack_btn) {
    lv_label_set_text(lbl_ack_btn, tr("Quittieren", "Acknowledge"));
    lv_obj_set_style_text_font(lbl_ack_btn, lang_en ? font_small() : font_body(), 0);
  }
  if(lbl_name_dist) {
    lv_label_set_text(lbl_name_dist, tr("Distanz", "Distance"));
  }
  if(lbl_name_temp) {
    lv_label_set_text(lbl_name_temp, tr("Temperatur", "Temperature"));
    lv_obj_set_style_text_font(lbl_name_temp, lang_en ? font_small() : font_body(), 0);
  }
  if(lbl_name_hum) {
    lv_label_set_text(lbl_name_hum, tr("Feuchte", "Humidity"));
  }
  if(lbl_sys_eth_k) {
    lv_label_set_text(lbl_sys_eth_k, "Ethernet");
  }
  if(has_sysmon && lbl_sys_cpu) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%d %%", last_cpu_pct);
    lv_label_set_text(lbl_sys_cpu, buf);
  }
  if(has_sysmon && lbl_sys_heap) {
    char buf[20];
    snprintf(buf, sizeof(buf), tr("%d %% frei", "%d %% free"), last_heap_free_pct);
    lv_label_set_text(lbl_sys_heap, buf);
  }
  if(lbl_fkey[0]) {
    lv_label_set_text(lbl_fkey[0], tr("Menü", "Menu"));
  }
  if(lbl_fkey[1]) {
    lv_label_set_text(lbl_fkey[1], tr("Kalibrierung", "Calibration"));
  }
  if(lbl_fkey[2]) {
    lv_label_set_text(lbl_fkey[2], tr("Protokoll", "Log"));
  }
  if(lbl_fkey[3]) {
    lv_label_set_text(lbl_fkey[3], "Alarm");
  }
  if(lbl_cal[0]) {
    lv_label_set_text(lbl_cal[0], tr("Temperatur Warn  5.0 ... 32.0 °C",
                                    "Temp warn      5.0 ... 32.0 °C"));
    lv_label_set_text(lbl_cal[1], tr("Temperatur Alarm 0.0 ... 40.0 °C",
                                    "Temp alarm     0.0 ... 40.0 °C"));
    lv_label_set_text(lbl_cal[2], tr("Feuchte Warn    25 ... 75 %",
                                    "Humidity warn  25 ... 75 %"));
    lv_label_set_text(lbl_cal[3], tr("Feuchte Alarm   15 ... 90 %",
                                    "Humidity alarm 15 ... 90 %"));
    lv_label_set_text(lbl_cal[4], tr("Distanz Warn    < 50 cm",
                                    "Distance warn  < 50 cm"));
    lv_label_set_text(lbl_cal[5], tr("Distanz Alarm   < 20 cm",
                                    "Distance alarm < 20 cm"));
  }
  show_section(section_idx);
  update_ack_session_ui();
  update_alarm_banner();
  update_system_row(lbl_sys_eth, link_eth, tr("Verbunden", "Connected"),
                    tr("Getrennt", "Offline"));
  evt_refresh_strip();
  if(evt_overlay != NULL) {
    evt_refresh_overlay_list();
  }
}

static void on_lang_de(lv_event_t *e)
{
  LV_UNUSED(e);
  if(lang_en) {
    lang_en = false;
    apply_lang();
  }
}

static void on_lang_en(lv_event_t *e)
{
  LV_UNUSED(e);
  if(!lang_en) {
    lang_en = true;
    apply_lang();
  }
}

void operation_hmi_create(void)
{
  T = PALETTE;
  zone_color = T.safe;
  fonts_init();

  scr_root = lv_screen_active();
  lv_obj_set_style_bg_color(scr_root, lv_color_hex(T.bg), 0);
  lv_obj_set_style_bg_opa(scr_root, LV_OPA_COVER, 0);
  lv_obj_set_flex_flow(scr_root, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(scr_root, 0, 0);
  lv_obj_set_style_pad_row(scr_root, 0, 0);
  lv_obj_clear_flag(scr_root, LV_OBJ_FLAG_SCROLLABLE);

  hdr_bar = lv_obj_create(scr_root);
  lv_obj_set_size(hdr_bar, LV_PCT(100), 28);
  lv_obj_set_style_bg_color(hdr_bar, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(hdr_bar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(hdr_bar, 0, 0);
  lv_obj_set_style_radius(hdr_bar, 0, 0);
  lv_obj_set_style_pad_hor(hdr_bar, 8, 0);
  lv_obj_set_style_pad_column(hdr_bar, 8, 0);
  lv_obj_set_flex_flow(hdr_bar, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(hdr_bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(hdr_bar, LV_OBJ_FLAG_SCROLLABLE);

  lbl_h1 = lv_label_create(hdr_bar);
  lv_label_set_text(lbl_h1, OPERATION_HMI_PRODUCT_NAME);
  lv_obj_set_style_text_color(lbl_h1, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(lbl_h1, font_small(), 0);
  lv_obj_set_width(lbl_h1, 92);
  lv_label_set_long_mode(lbl_h1, LV_LABEL_LONG_CLIP);

  lv_obj_t *lang = lv_obj_create(hdr_bar);
  lv_obj_set_size(lang, 52, 18);
  lv_obj_set_style_bg_color(lang, lv_color_hex(0x2A2A2A), 0);
  lv_obj_set_style_bg_opa(lang, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(lang, 0, 0);
  lv_obj_set_style_radius(lang, 2, 0);
  lv_obj_set_style_pad_all(lang, 0, 0);
  lv_obj_set_style_pad_column(lang, 2, 0);
  lv_obj_set_flex_flow(lang, LV_FLEX_FLOW_ROW);
  lv_obj_clear_flag(lang, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_margin_right(lang, 4, 0);

  btn_lang_de = lv_button_create(lang);
  lv_obj_set_size(btn_lang_de, 24, 18);
  lv_obj_set_style_radius(btn_lang_de, 2, 0);
  lv_obj_set_style_pad_all(btn_lang_de, 0, 0);
  style_quiet_btn(btn_lang_de);
  lv_obj_add_event_cb(btn_lang_de, on_lang_de, LV_EVENT_CLICKED, NULL);
  lv_obj_t *ld = lv_label_create(btn_lang_de);
  lv_label_set_text(ld, "DE");
  lv_obj_set_style_text_color(ld, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(ld, font_small(), 0);
  lv_obj_center(ld);

  btn_lang_en = lv_button_create(lang);
  lv_obj_set_size(btn_lang_en, 24, 18);
  lv_obj_set_style_radius(btn_lang_en, 2, 0);
  lv_obj_set_style_pad_all(btn_lang_en, 0, 0);
  style_quiet_btn(btn_lang_en);
  lv_obj_add_event_cb(btn_lang_en, on_lang_en, LV_EVENT_CLICKED, NULL);
  lv_obj_t *le = lv_label_create(btn_lang_en);
  lv_label_set_text(le, "EN");
  lv_obj_set_style_text_color(le, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(le, font_small(), 0);
  lv_obj_center(le);

  lbl_h2 = lv_label_create(hdr_bar);
  lv_label_set_text(lbl_h2, "Betriebsübersicht");
  lv_obj_set_style_text_color(lbl_h2, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(lbl_h2, font_body(), 0);
  lv_obj_set_style_text_align(lbl_h2, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_flex_grow(lbl_h2, 1);
  lv_obj_set_style_min_width(lbl_h2, 0, 0);
  lv_label_set_long_mode(lbl_h2, LV_LABEL_LONG_CLIP);

  lv_obj_t *right = lv_obj_create(hdr_bar);
  lv_obj_set_size(right, 168, 28);
  lv_obj_set_style_min_width(right, 168, 0);
  style_plain(right);
  lv_obj_set_flex_flow(right, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(right, 5, 0);
  lv_obj_set_flex_align(right, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  alarm_led = lv_obj_create(right);
  style_plain(alarm_led);
  lv_obj_set_size(alarm_led, 12, 12);
  lv_obj_set_style_radius(alarm_led, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(alarm_led, lv_color_hex(T.safe), 0);
  lv_obj_set_style_bg_opa(alarm_led, LV_OPA_COVER, 0);

  lbl_betrieb = lv_label_create(right);
  lv_label_set_text(lbl_betrieb, "BETRIEB");
  lv_obj_set_style_text_color(lbl_betrieb, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(lbl_betrieb, font_small(), 0);

  lbl_clock = lv_label_create(right);
  lv_label_set_text(lbl_clock, "--:--:--");
  lv_obj_set_style_text_color(lbl_clock, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(lbl_clock, font_small(), 0);

  lv_obj_t *pages = lv_obj_create(scr_root);
  lv_obj_set_width(pages, LV_PCT(100));
  lv_obj_set_flex_grow(pages, 1);
  style_plain(pages);
  lv_obj_set_style_pad_all(pages, 6, 0);

  page_home = lv_obj_create(pages);
  lv_obj_set_size(page_home, LV_PCT(100), LV_PCT(100));
  style_plain(page_home);
  lv_obj_set_style_pad_column(page_home, 6, 0);
  lv_obj_set_flex_flow(page_home, LV_FLEX_FLOW_ROW);
  lv_obj_align(page_home, LV_ALIGN_TOP_LEFT, 0, 0);

  /* ----- ANLAGE ----- */
  lv_obj_t *anlage = make_col_card(page_home, "ANLAGE", 128, &ttl_anlage);
  lv_obj_t *ain = lv_obj_create(anlage);
  lv_obj_set_width(ain, LV_PCT(100));
  lv_obj_set_flex_grow(ain, 1);
  style_plain(ain);
  lv_obj_set_style_pad_all(ain, 8, 0);
  lv_obj_set_style_pad_row(ain, 8, 0);
  lv_obj_set_flex_flow(ain, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(ain, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  box_ready = lv_obj_create(ain);
  lv_obj_set_size(box_ready, LV_PCT(100), 56);
  lv_obj_set_style_bg_color(box_ready, lv_color_hex(T.safe), 0);
  lv_obj_set_style_bg_opa(box_ready, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(box_ready, 8, 0);
  lv_obj_set_style_border_width(box_ready, 0, 0);
  lv_obj_clear_flag(box_ready, LV_OBJ_FLAG_SCROLLABLE);
  lbl_ready = lv_label_create(box_ready);
  lv_label_set_text(lbl_ready, "BEREIT");
  lv_obj_set_style_text_color(lbl_ready, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(lbl_ready, font_big(), 0);
  lv_obj_center(lbl_ready);

  lv_obj_t *strow = lv_obj_create(ain);
  lv_obj_set_size(strow, LV_PCT(100), LV_SIZE_CONTENT);
  style_plain(strow);
  lv_obj_set_flex_flow(strow, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(strow, 4, 0);
  lbl_stoerung = lv_label_create(strow);
  lv_label_set_text(lbl_stoerung, "Störung:");
  lv_obj_set_style_text_color(lbl_stoerung, lv_color_hex(T.title), 0);
  lv_obj_set_style_text_font(lbl_stoerung, font_small(), 0);
  lbl_stoerung_val = lv_label_create(strow);
  lv_label_set_text(lbl_stoerung_val, "keine");
  lv_obj_set_style_text_color(lbl_stoerung_val, lv_color_hex(T.safe), 0);
  lv_obj_set_style_text_font(lbl_stoerung_val, font_small(), 0);
  lv_label_set_long_mode(lbl_stoerung_val, LV_LABEL_LONG_DOT);
  lv_obj_set_flex_grow(lbl_stoerung_val, 1);

  btn_ack = lv_button_create(ain);
  lv_obj_set_size(btn_ack, LV_PCT(100), 32);
  lv_obj_set_style_bg_color(btn_ack, lv_color_hex(0xE4E4E4), 0);
  lv_obj_set_style_bg_opa(btn_ack, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(btn_ack, 0, 0);
  lv_obj_set_style_border_width(btn_ack, 1, 0);
  lv_obj_set_style_border_color(btn_ack, lv_color_hex(0x222222), 0);
  style_quiet_btn(btn_ack);
  lv_obj_add_event_cb(btn_ack, on_ack, LV_EVENT_CLICKED, NULL);
  lbl_ack_btn = lv_label_create(btn_ack);
  lv_label_set_text(lbl_ack_btn, "Quittieren");
  lv_obj_set_style_text_color(lbl_ack_btn, lv_color_hex(T.title), 0);
  lv_obj_set_style_text_font(lbl_ack_btn, font_body(), 0);
  lv_obj_center(lbl_ack_btn);

  lv_obj_t *sp = lv_obj_create(ain);
  lv_obj_set_width(sp, LV_PCT(100));
  lv_obj_set_flex_grow(sp, 1);
  style_plain(sp);

  lbl_ack_timer = lv_label_create(ain);
  lv_label_set_text(lbl_ack_timer, "Operator-Freigabe");
  lv_obj_set_style_text_color(lbl_ack_timer, lv_color_hex(T.muted), 0);
  lv_obj_set_style_text_font(lbl_ack_timer, font_small(), 0);
  lv_label_set_long_mode(lbl_ack_timer, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(lbl_ack_timer, LV_PCT(100));
  lv_obj_set_style_text_align(lbl_ack_timer, LV_TEXT_ALIGN_CENTER, 0);
  lbl_ack_status = lv_label_create(ain);
  lv_label_set_text(lbl_ack_status, "");
  lv_obj_set_style_text_color(lbl_ack_status, lv_color_hex(T.muted), 0);
  lv_obj_set_style_text_font(lbl_ack_status, font_small(), 0);
  lv_label_set_long_mode(lbl_ack_status, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(lbl_ack_status, LV_PCT(100));

  /* ----- MESSWERTE ----- */
  lv_obj_t *mess = make_col_card(page_home, "MESSWERTE", 230, &ttl_mess);
  lv_obj_set_flex_grow(mess, 1);
  lv_obj_clear_flag(mess, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  lv_obj_clear_flag(mess, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(mess, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_clip_corner(mess, true, 0);
  lv_obj_t *m_dist = make_metric(mess, "Distanz", "cm", &lbl_name_dist, &lbl_dist, &lbl_dist_unit, &bar_dist,
              0, DIST_BAR_MAX, true, "0", "50", "100");
  lv_obj_t *m_temp = make_metric(mess, "Temperatur", "°C", &lbl_name_temp, &lbl_temp, &lbl_temp_unit, &bar_temp,
              TEMP_BAR_MIN, TEMP_BAR_MAX, false, "-20", "0", "80");
  lv_obj_t *m_hum = make_metric(mess, "Feuchte", "%", &lbl_name_hum, &lbl_hum, &lbl_hum_unit, &bar_hum,
              0, 100, false, "0", "50", "100");
  lv_obj_set_style_border_width(m_hum, 0, 0);
  (void)m_dist;
  (void)m_temp;

  /* ----- SYSTEM + EREIGNISSE ----- */
  lv_obj_t *rightcol = lv_obj_create(page_home);
  lv_obj_set_size(rightcol, 132, LV_PCT(100));
  style_plain(rightcol);
  lv_obj_set_flex_flow(rightcol, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(rightcol, 6, 0);

  lv_obj_t *sys = make_col_card(rightcol, "SYSTEM", LV_PCT(100), &ttl_sys);
  lv_obj_set_height(sys, 152);
  make_sys_row(sys, "Ethernet", &lbl_sys_eth, "Getrennt", &lbl_sys_eth_k);
  make_sys_row(sys, "FreeRTOS", &lbl_sys_f756, "OK", NULL);
  make_sys_row(sys, "Watchdog", &lbl_sys_wd, "OK", NULL);
  make_sys_row(sys, "CPU", &lbl_sys_cpu, "—", NULL);
  make_sys_row(sys, "Heap", &lbl_sys_heap, "—", &lbl_sys_heap_k);
  lv_obj_set_style_text_color(lbl_sys_eth, lv_color_hex(T.alarm), 0);
  lv_obj_set_style_text_color(lbl_sys_f756, lv_color_hex(T.safe), 0);
  lv_obj_set_style_text_color(lbl_sys_wd, lv_color_hex(T.safe), 0);
  lv_obj_set_style_text_color(lbl_sys_cpu, lv_color_hex(T.title), 0);
  lv_obj_set_style_text_color(lbl_sys_heap, lv_color_hex(T.title), 0);

  lv_obj_t *ereig = make_col_card(rightcol, "EREIGNISSE", LV_PCT(100), &ttl_ereig);
  lv_obj_set_flex_grow(ereig, 1);
  lv_obj_t *elist = lv_obj_create(ereig);
  lv_obj_set_width(elist, LV_PCT(100));
  lv_obj_set_flex_grow(elist, 1);
  style_plain(elist);
  lv_obj_set_style_pad_all(elist, 6, 0);
  lv_obj_set_style_pad_row(elist, 2, 0);
  lv_obj_set_flex_flow(elist, LV_FLEX_FLOW_COLUMN);
  for(int i = 0; i < EVT_VIEW; i++) {
    evt_lines[i] = lv_label_create(elist);
    lv_label_set_text(evt_lines[i], "");
    lv_obj_set_style_text_color(evt_lines[i], lv_color_hex(T.title), 0);
    lv_obj_set_style_text_font(evt_lines[i], font_small(), 0);
    lv_label_set_long_mode(evt_lines[i], LV_LABEL_LONG_CLIP);
    lv_obj_set_width(evt_lines[i], LV_PCT(100));
  }

  /* Calibration page */
  page_cal = lv_obj_create(pages);
  lv_obj_set_size(page_cal, LV_PCT(100), LV_PCT(100));
  style_plain(page_cal);
  lv_obj_add_flag(page_cal, LV_OBJ_FLAG_HIDDEN);
  lv_obj_align(page_cal, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_t *calc = make_col_card(page_cal, "KALIBRIERUNG", LV_PCT(100), &ttl_cal);
  lv_obj_t *cin = lv_obj_create(calc);
  lv_obj_set_width(cin, LV_PCT(100));
  lv_obj_set_flex_grow(cin, 1);
  style_plain(cin);
  lv_obj_set_style_pad_all(cin, 12, 0);
  lv_obj_set_style_pad_row(cin, 8, 0);
  lv_obj_set_flex_flow(cin, LV_FLEX_FLOW_COLUMN);
  const char *cal_lines[] = {
      "Temperatur Warn  5.0 ... 32.0 °C",
      "Temperatur Alarm 0.0 ... 40.0 °C",
      "Feuchte Warn    25 ... 75 %",
      "Feuchte Alarm   15 ... 90 %",
      "Distanz Warn    < 50 cm",
      "Distanz Alarm   < 20 cm",
  };
  for(unsigned i = 0; i < sizeof(cal_lines) / sizeof(cal_lines[0]); i++) {
    lv_obj_t *l = lv_label_create(cin);
    lv_label_set_text(l, cal_lines[i]);
    lv_obj_set_style_text_color(l, lv_color_hex(T.title), 0);
    lv_obj_set_style_text_font(l, font_body(), 0);
    lbl_cal[i] = l;
  }

  /* Alarm page */
  page_alarm = lv_obj_create(pages);
  lv_obj_set_size(page_alarm, LV_PCT(100), LV_PCT(100));
  style_plain(page_alarm);
  lv_obj_add_flag(page_alarm, LV_OBJ_FLAG_HIDDEN);
  lv_obj_align(page_alarm, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_t *almc = make_col_card(page_alarm, "ALARM", LV_PCT(100), &ttl_alarm);
  lv_obj_t *alin = lv_obj_create(almc);
  lv_obj_set_width(alin, LV_PCT(100));
  lv_obj_set_flex_grow(alin, 1);
  style_plain(alin);
  lv_obj_set_style_pad_all(alin, 12, 0);
  lv_obj_set_style_pad_row(alin, 10, 0);
  lv_obj_set_flex_flow(alin, LV_FLEX_FLOW_COLUMN);
  lbl_alarm_pri = lv_label_create(alin);
  lv_label_set_text(lbl_alarm_pri, "BEREIT");
  lv_obj_set_style_text_color(lbl_alarm_pri, lv_color_hex(T.safe), 0);
  lv_obj_set_style_text_font(lbl_alarm_pri, font_big(), 0);
  lbl_alarm_txt = lv_label_create(alin);
  lv_label_set_text(lbl_alarm_txt, "Keine aktive Störung.");
  lv_obj_set_style_text_color(lbl_alarm_txt, lv_color_hex(T.title), 0);
  lv_obj_set_style_text_font(lbl_alarm_txt, font_body(), 0);
  lv_label_set_long_mode(lbl_alarm_txt, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(lbl_alarm_txt, LV_PCT(100));
  lbl_zone = lv_label_create(alin);
  lv_label_set_text(lbl_zone, "SAFE");
  lv_obj_set_style_text_font(lbl_zone, font_mid(), 0);
  lbl_status = lv_label_create(alin);
  lv_label_set_text(lbl_status, "");
  lv_obj_set_style_text_color(lbl_status, lv_color_hex(T.muted), 0);
  lv_obj_set_style_text_font(lbl_status, font_small(), 0);
  lv_label_set_long_mode(lbl_status, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(lbl_status, LV_PCT(100));

  /* Footer F-keys */
  nav_bar = lv_obj_create(scr_root);
  lv_obj_set_size(nav_bar, LV_PCT(100), 36);
  lv_obj_set_style_bg_color(nav_bar, lv_color_hex(T.footer), 0);
  lv_obj_set_style_bg_opa(nav_bar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_side(nav_bar, LV_BORDER_SIDE_TOP, 0);
  lv_obj_set_style_border_width(nav_bar, 1, 0);
  lv_obj_set_style_border_color(nav_bar, lv_color_hex(0xB0B0B0), 0);
  lv_obj_set_style_radius(nav_bar, 0, 0);
  lv_obj_set_style_pad_all(nav_bar, 0, 0);
  lv_obj_set_flex_flow(nav_bar, LV_FLEX_FLOW_ROW);
  lv_obj_clear_flag(nav_bar, LV_OBJ_FLAG_SCROLLABLE);

  fkey[0] = make_fkey(nav_bar, "Menü", on_f1, &lbl_fkey[0]);
  fkey[1] = make_fkey(nav_bar, "Kalibrierung", on_f2, &lbl_fkey[1]);
  fkey[2] = make_fkey(nav_bar, "Protokoll", on_f3, &lbl_fkey[2]);
  fkey[3] = make_fkey(nav_bar, "Alarm", on_f4, &lbl_fkey[3]);
  lv_obj_set_style_border_width(fkey[3], 0, 0);

  show_section(0);
  operation_hmi_set_link_f756(false);
  operation_hmi_set_link_f401(false);
  operation_hmi_set_link_eth(false);
  operation_hmi_set_link_nfc(false);
  operation_hmi_set_ack_armed(false);
  links_inited = true;
  update_alarm_banner();

  clock_timer = lv_timer_create(clock_timer_cb, 1000, NULL);
  clock_timer_cb(NULL);
  operation_hmi_log_event(OPERATION_HMI_EVT_OK, "Systemstart");
  apply_lang();
}

void operation_hmi_log_event(operation_hmi_evt_level_t level, const char *text)
{
  if(text == NULL || text[0] == '\0') {
    return;
  }
  hmi_evt_t *slot = &evt_ring[evt_head];
  slot->level = level;
  slot->tick_ms = lv_tick_get();
  strncpy(slot->text, text, EVT_TEXT - 1);
  slot->text[EVT_TEXT - 1] = '\0';
  evt_head = (uint8_t)((evt_head + 1) % EVT_CAP);
  if(evt_count < EVT_CAP) {
    evt_count++;
  }
  evt_refresh_strip();
  if(evt_overlay != NULL) {
    evt_refresh_overlay_list();
  }
}

void operation_hmi_set_temp_c(float temp_c)
{
  if(lbl_temp == NULL) {
    return;
  }
  char buf[16];
  /* nano.specs has no float printf unless -u _printf_float — use ints */
  {
    const int t10 = (int)(temp_c * 10.0f);
    const int t_abs = (t10 < 0) ? -t10 : t10;
    snprintf(buf, sizeof(buf), "%s%d.%d", (t10 < 0) ? "-" : "", t_abs / 10, t_abs % 10);
  }
  lv_label_set_text(lbl_temp, buf);
  last_temp_c = temp_c;
  has_temp = true;
  if(bar_temp) {
    lv_bar_set_value(bar_temp, clamp_i((int32_t)(temp_c + (temp_c >= 0 ? 0.5f : -0.5f)),
                                      TEMP_BAR_MIN, TEMP_BAR_MAX), LV_ANIM_OFF);
  }
  update_alarm_banner();
}

void operation_hmi_set_hum_pct(float hum_pct)
{
  if(lbl_hum == NULL) {
    return;
  }
  char buf[16];
  {
    const int h10 = (int)(hum_pct * 10.0f);
    const int h_abs = (h10 < 0) ? -h10 : h10;
    snprintf(buf, sizeof(buf), "%d.%d", h_abs / 10, h_abs % 10);
  }
  lv_label_set_text(lbl_hum, buf);
  last_hum_pct = hum_pct;
  has_hum = true;
  if(bar_hum) {
    lv_bar_set_value(bar_hum, clamp_i((int32_t)(hum_pct + 0.5f), 0, 100), LV_ANIM_OFF);
  }
  update_alarm_banner();
}

void operation_hmi_set_distance_cm(int distance_cm)
{
  if(lbl_dist == NULL) {
    return;
  }
  last_dist_cm = distance_cm;
  if(distance_cm < 0) {
    lv_label_set_text(lbl_dist, "--");
    if(bar_dist) {
      lv_bar_set_value(bar_dist, 0, LV_ANIM_OFF);
    }
    update_alarm_banner();
    return;
  }
  char buf[16];
  snprintf(buf, sizeof(buf), "%d", distance_cm);
  lv_label_set_text(lbl_dist, buf);
  lv_obj_invalidate(lbl_dist);
  if(bar_dist) {
    lv_bar_set_value(bar_dist, clamp_i(distance_cm, 0, DIST_BAR_MAX), LV_ANIM_OFF);
  }
  if(chart_dist && ser_dist) {
    lv_chart_set_next_value(chart_dist, ser_dist, clamp_i(distance_cm, 0, DIST_BAR_MAX));
  }
  update_alarm_banner();
}

void operation_hmi_set_zone(const char *zone_label)
{
  if(zone_label == NULL) {
    return;
  }
  const int changed = (strcmp(last_zone_logged, zone_label) != 0);
  zone_kind = zone_label[0];
  if(zone_kind != 'W' && zone_kind != 'A' && zone_kind != 'F') {
    zone_kind = 'S';
  }
  zone_color = zone_color_from_kind(zone_kind);
  update_alarm_banner();
  if(changed && zone_label[0] != '\0' && strcmp(zone_label, "—") != 0) {
    strncpy(last_zone_logged, zone_label, sizeof(last_zone_logged) - 1);
    last_zone_logged[sizeof(last_zone_logged) - 1] = '\0';
    char msg[40];
    snprintf(msg, sizeof(msg), "Zone %s", zone_label);
    operation_hmi_evt_level_t lvl = OPERATION_HMI_EVT_OK;
    if(zone_kind == 'W') {
      lvl = OPERATION_HMI_EVT_WARN;
    } else if(zone_kind == 'A' || zone_kind == 'F') {
      lvl = OPERATION_HMI_EVT_ALARM;
    }
    operation_hmi_log_event(lvl, msg);
  }
}

void operation_hmi_set_status(const char *status_line)
{
  if(lbl_status == NULL || status_line == NULL) {
    return;
  }
  lv_label_set_text(lbl_status, status_line);
}

void operation_hmi_set_ack_status(const char *text)
{
  if(lbl_ack_status == NULL || text == NULL) {
    return;
  }
  if(ack_clear_timer != NULL) {
    lv_timer_delete(ack_clear_timer);
    ack_clear_timer = NULL;
  }
  lv_label_set_text(lbl_ack_status, text);

  uint32_t color = T.muted;
  operation_hmi_evt_level_t lvl = OPERATION_HMI_EVT_INFO;
  const int failed = (strstr(text, "denied") != NULL) || (strstr(text, "fail") != NULL);
  if(failed) {
    color = T.alarm;
    lvl = OPERATION_HMI_EVT_ALARM;
  } else if(strstr(text, "tap") != NULL || strstr(text, "locked") != NULL) {
    color = T.alarm;
    lvl = OPERATION_HMI_EVT_WARN;
  } else if(strstr(text, "armed") != NULL || strstr(text, "ACK") != NULL ||
            strstr(text, "ack") != NULL || strstr(text, "badge") != NULL) {
    color = T.header;
    lvl = OPERATION_HMI_EVT_WARN;
  }
  lv_obj_set_style_text_color(lbl_ack_status, lv_color_hex(color), 0);

  if(text[0] != '\0' && strcmp(text, last_ack_logged) != 0 &&
     strstr(text, "sending") == NULL && strstr(text, "waiting") == NULL) {
    strncpy(last_ack_logged, text, sizeof(last_ack_logged) - 1);
    last_ack_logged[sizeof(last_ack_logged) - 1] = '\0';
    operation_hmi_log_event(lvl, text);
  }
  if(failed) {
    ack_dlg_show(false, tr("QUITTIERUNG FEHLER", "ACKNOWLEDGE ERROR"), text);
  }
}

void operation_hmi_set_operator_id(const char *uid)
{
  if(uid == NULL || uid[0] == '\0') {
    operator_id[0] = '\0';
    return;
  }
  strncpy(operator_id, uid, sizeof(operator_id) - 1);
  operator_id[sizeof(operator_id) - 1] = '\0';
}

void operation_hmi_confirm_ack(bool ok, const char *who)
{
  const char *op = (who != NULL && who[0] != '\0' && strcmp(who, "NONE") != 0)
                       ? who
                       : (operator_id[0] ? operator_id : "LOCAL");
  if(op[0] != '\0' && strcmp(op, "LOCAL") != 0 && strcmp(op, "F756") != 0) {
    operation_hmi_set_operator_id(op);
  }
  if(!ok) {
    operation_hmi_set_ack_status("ACK denied");
    return;
  }

  alarm_src_t src;
  int sev;
  char msg[56];
  evaluate_alarm(&src, &sev, msg, sizeof(msg));
  (void)src;

  if(sev >= 1 && alarm_acked) {
    operation_hmi_set_ack_status(tr("bereits quittiert", "already acked"));
    return;
  }

  alarm_acked = (sev >= 1);
  update_alarm_banner();

  char log[EVT_TEXT];
  char dlg[96];
  if(sev >= 1) {
    snprintf(log, sizeof(log), tr("Quittiert %s %s", "Acked %s %s"), msg, op);
    snprintf(dlg, sizeof(dlg), tr("%s\nOperator %s\nBleibt bis Prozess i.O.",
                                 "%s\nOperator %s\nStays until process OK"), msg, op);
    operation_hmi_log_event(sev >= 2 ? OPERATION_HMI_EVT_ALARM : OPERATION_HMI_EVT_WARN, log);
    if(lbl_ack_status) {
      lv_label_set_text(lbl_ack_status, tr("quittiert", "acked"));
      lv_obj_set_style_text_color(lbl_ack_status, lv_color_hex(T.safe), 0);
    }
    ack_dlg_show(true, tr("QUITTIERUNG", "ACKNOWLEDGE"), dlg);
  } else {
    snprintf(log, sizeof(log), tr("Quittiert keine Störung %s", "Acked no fault %s"), op);
    snprintf(dlg, sizeof(dlg), tr("Keine aktive Störung.\nOperator %s",
                                 "No active fault.\nOperator %s"), op);
    operation_hmi_log_event(OPERATION_HMI_EVT_OK, log);
    if(lbl_ack_status) {
      lv_label_set_text(lbl_ack_status, tr("keine Störung", "no fault"));
      lv_obj_set_style_text_color(lbl_ack_status, lv_color_hex(T.muted), 0);
    }
    ack_dlg_show(true, tr("QUITTIERUNG", "ACKNOWLEDGE"), dlg);
  }
}

static void link_log(const char *name, bool was, bool now)
{
  if(!links_inited || was == now) {
    return;
  }
  char msg[40];
  snprintf(msg, sizeof(msg), "%s %s", name, now ? "up" : "down");
  operation_hmi_log_event(now ? OPERATION_HMI_EVT_OK : OPERATION_HMI_EVT_WARN, msg);
}

void operation_hmi_set_link_f756(bool online)
{
  link_log("F756", link_f756, online);
  link_f756 = online;
}

void operation_hmi_set_link_f401(bool online)
{
  link_log("F401", link_f401, online);
  link_f401 = online;
}

void operation_hmi_set_link_eth(bool online)
{
  link_log("Ethernet", link_eth, online);
  link_eth = online;
  update_system_row(lbl_sys_eth, online, tr("Verbunden", "Connected"), tr("Getrennt", "Offline"));
}

void operation_hmi_set_link_nfc(bool online)
{
  link_log("NFC", link_nfc, online);
  link_nfc = online;
}

void operation_hmi_set_cpu_pct(int cpu_pct)
{
  if(cpu_pct < 0) {
    cpu_pct = 0;
  }
  if(cpu_pct > 100) {
    cpu_pct = 100;
  }
  last_cpu_pct = cpu_pct;
  has_sysmon = true;
  if(lbl_sys_cpu == NULL) {
    return;
  }
  char buf[12];
  snprintf(buf, sizeof(buf), "%d %%", cpu_pct);
  lv_label_set_text(lbl_sys_cpu, buf);
}

void operation_hmi_set_heap_free_pct(int free_pct)
{
  if(free_pct < 0) {
    free_pct = 0;
  }
  if(free_pct > 100) {
    free_pct = 100;
  }
  last_heap_free_pct = free_pct;
  has_sysmon = true;
  if(lbl_sys_heap == NULL) {
    return;
  }
  char buf[20];
  snprintf(buf, sizeof(buf), tr("%d %% frei", "%d %% free"), free_pct);
  lv_label_set_text(lbl_sys_heap, buf);
}

void operation_hmi_set_ack_armed(bool armed)
{
  if(ack_armed_state != armed && links_inited) {
    operation_hmi_log_event(armed ? OPERATION_HMI_EVT_OK : OPERATION_HMI_EVT_INFO,
                            armed ? "Freigabe aktiv" : "Freigabe gesperrt");
  }
  ack_armed_state = armed;
  apply_ack_armed_style();
  if(armed) {
    ack_remain_s = ACK_SESSION_S;
    if(ack_session_timer == NULL) {
      ack_session_timer = lv_timer_create(ack_session_timer_cb, 1000, NULL);
    }
  } else {
    ack_remain_s = 0;
    if(ack_session_timer != NULL) {
      lv_timer_delete(ack_session_timer);
      ack_session_timer = NULL;
    }
  }
  update_ack_session_ui();
}
