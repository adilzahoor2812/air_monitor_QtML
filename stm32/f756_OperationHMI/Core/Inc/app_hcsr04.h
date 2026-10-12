/**
 * @file app_hcsr04.h
 * @brief HC-SR04 on NUCLEO-F756ZG — TRIG D8, ECHO D9 (auto-accepts swap).
 */
#ifndef APP_HCSR04_H
#define APP_HCSR04_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  uint32_t distance_cm;
  uint32_t echo_us;
  uint32_t timestamp_ms;
  uint8_t valid;
} AppHcsr04Sample_t;

void AppHcsr04_SetLog(void (*fn)(const char *msg));
void AppHcsr04_Init(void);
int AppHcsr04_Read(AppHcsr04Sample_t *out); /* 0=ok, <0 fail */
int AppHcsr04_LastError(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_HCSR04_H */
