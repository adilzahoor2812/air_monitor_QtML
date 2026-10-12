/**
 * @file app_pn532.h
 * @brief PN532 NFC — soft-UART on D14/D15 (same pins as I2C), USART6 D0/D1, or I2C.
 *
 * Elechouse V3: SDA/SCL (front) = TXD/RXD (back). DIP OFF/OFF = UART.
 *
 * Recommended UART wiring (keep on D14/D15):
 *   VCC→3V3  GND→GND  TXD/SDA→D14  RXD/SCL→D15
 *
 * Optional RSTO→D3
 */
#ifndef APP_PN532_H
#define APP_PN532_H

#include "stm32f7xx_hal.h"
#include <stdint.h>

#define APP_PN532_UID_MAX 10U

#define APP_PN532_RST_GPIO_Port GPIOE
#define APP_PN532_RST_Pin       GPIO_PIN_13

typedef enum
{
  APP_PN532_BUS_NONE = 0,
  APP_PN532_BUS_I2C,
  APP_PN532_BUS_UART,   /* USART6 D0/D1 */
  APP_PN532_BUS_SUART,  /* bit-bang on D14/D15 */
  APP_PN532_BUS_SPI     /* soft-SPI D10/D13/D12/D3 */
} AppPn532Bus_t;

typedef struct
{
  uint8_t uid[APP_PN532_UID_MAX];
  uint8_t uid_len;
} AppPn532Uid_t;

void AppPn532_HwReset(void);
void AppPn532_SetLog(void (*log_fn)(const char *s));

/** Prefer soft-UART on D14/D15, then USART6, then I2C. */
int AppPn532_Init(I2C_HandleTypeDef *hi2c, UART_HandleTypeDef *huart);

int AppPn532_IsReady(void);
AppPn532Bus_t AppPn532_GetBus(void);
int AppPn532_PollUid(AppPn532Uid_t *out);

#endif /* APP_PN532_H */
