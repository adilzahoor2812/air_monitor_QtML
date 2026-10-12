/**
 * @file app_dht22.h
 * @brief DHT22 (AM2302) one-wire driver — data on PF15 (Nucleo Arduino D2).
 */
#ifndef APP_DHT22_H
#define APP_DHT22_H

#include <stdint.h>

typedef struct
{
  float temperature_c;
  float humidity_pct;
  uint8_t valid;
} AppDht22Sample_t;

void AppDht22_Init(void);

/**
 * @brief Read one sample. Blocking ~20 ms + bus wait.
 * @return 0 on success, non-zero on timeout/checksum error.
 */
int AppDht22_Read(AppDht22Sample_t *out);

#endif /* APP_DHT22_H */
