/**
 * @file app_lvgl.h
 * @brief LVGL 9 port: SPI flush + tick. Call AppLvgl_Init then operation_hmi_create.
 */
#ifndef APP_LVGL_H
#define APP_LVGL_H

#ifdef __cplusplus
extern "C" {
#endif

void AppLvgl_Init(void);
void AppLvgl_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_LVGL_H */
