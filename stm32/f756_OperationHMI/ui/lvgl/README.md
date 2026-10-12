# LVGL UI (real — deploy to display)

Portable sources: `operation_hmi.c` / `operation_hmi.h`  
Same code runs in the Mac SDL simulator and later on the F756 TFT.

## Mac simulator (LVGL window, live serial)

```bash
cd ../simulator
git clone --depth 1 --branch v9.2.2 https://github.com/lvgl/lvgl.git third_party/lvgl
cp lv_conf.h third_party/lvgl/lv_conf.h
cmake -S . -B build && cmake --build build -j
# close Terminal screen/cat on Nucleo first
./build/operation_hmi_sim
```

## F756 + display deploy

1. Add LVGL 9 to CubeIDE project.
2. Bring up SPI display (ILI9488 480×320 recommended) + flush callback.
3. Add these two files to the firmware build.
4. After `lv_init()` + display:

```c
#include "operation_hmi.h"

operation_hmi_create();

/* when DHT22 sample is ready: */
operation_hmi_set_temp_c(s.temperature_c);
operation_hmi_set_hum_pct(s.humidity_pct);
/* also appends points to TEMP/HUM trend charts */
operation_hmi_set_status("DHT22 OK");
operation_hmi_set_link_f756(true);

/* in main loop / FreeRTOS task: */
lv_timer_handler();
```

No browser — this is the firmware UI.
