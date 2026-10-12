/**
 * @file operation_hmi.h
 * @brief Portable Air Monitor HMI (LVGL 9) — same sources for SDL sim + F756 display.
 *
 * Target panel: 480 x 320 (ILI9488 / similar).
 */
#ifndef OPERATION_HMI_H
#define OPERATION_HMI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/** On-panel product name — industrial ambient air monitoring. */
#define OPERATION_HMI_PRODUCT_NAME     "AIR MONITOR"
#define OPERATION_HMI_PRODUCT_TAGLINE  "INDUSTRIAL AIR MONITORING"

/** Create the dashboard on the active screen. Call once after lv_init + display. */
void operation_hmi_create(void);

void operation_hmi_set_temp_c(float temp_c);
void operation_hmi_set_hum_pct(float hum_pct);
void operation_hmi_set_distance_cm(int distance_cm); /* <0 = unknown */
void operation_hmi_set_zone(const char *zone_label); /* "SAFE" / "WARN" / "FAULT" */
void operation_hmi_set_status(const char *status_line);
/** Short status under ACK (sending / denied / waiting). */
void operation_hmi_set_ack_status(const char *text);
void operation_hmi_set_link_f756(bool online);
void operation_hmi_set_link_f401(bool online);
void operation_hmi_set_link_eth(bool online);
void operation_hmi_set_link_nfc(bool online);

/** CPU busy % over the last firmware sample window (0–100). */
void operation_hmi_set_cpu_pct(int cpu_pct);
/** Newlib heap remaining % (0–100). */
void operation_hmi_set_heap_free_pct(int free_pct);

/** Last NFC badge UID used as operator ID on Quittierung (ACK). */
void operation_hmi_set_operator_id(const char *uid);

/**
 * IEC 62682 / VDI 3699 Quittierung: silence the active alarm.
 * Does not clear the banner while the process is still out of range.
 */
void operation_hmi_confirm_ack(bool ok, const char *operator_id);

/** Dim / enable ACK — false until NFC badge unlocks the session. */
void operation_hmi_set_ack_armed(bool armed);

/** Operator / system event levels for the alarm-event log. */
typedef enum {
  OPERATION_HMI_EVT_INFO = 0,
  OPERATION_HMI_EVT_OK,
  OPERATION_HMI_EVT_WARN,
  OPERATION_HMI_EVT_ALARM
} operation_hmi_evt_level_t;

/** Push an entry into the event log (newest first). Safe anytime after create. */
void operation_hmi_log_event(operation_hmi_evt_level_t level, const char *text);

/** Optional: called when operator presses ACK (register before create or after). */
typedef void (*operation_hmi_ack_cb_t)(void);
void operation_hmi_set_ack_callback(operation_hmi_ack_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif /* OPERATION_HMI_H */
