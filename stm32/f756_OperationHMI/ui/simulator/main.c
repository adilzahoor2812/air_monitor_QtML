/**
 * @file main.c
 * @brief LVGL SDL simulator — same UI as F756 display, fed by USART3 sensor lines.
 *
 * Close any Terminal `screen`/`cat` on the Nucleo port first, then:
 *   ./operation_hmi_sim
 *   ./operation_hmi_sim /dev/cu.usbmodem1103
 */
#define SDL_MAIN_HANDLED
#include "lvgl.h"
#include "operation_hmi.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <dirent.h>
#include <poll.h>
#include <SDL.h>

#define HOR_RES 480
#define VER_RES 320

typedef struct {
  float temp_c;
  float hum_pct;
  int distance_cm;
  int cpu_pct;
  int heap_free_pct;
  char zone[16];
  char status[96];
  char ack_status[48];
  bool has_temp;
  bool has_sysmon;
  bool link_f756;
  bool link_nfc;
  bool ack_armed;
  bool dirty;
  bool ack_dirty;
  pthread_mutex_t lock;
} live_t;

static live_t g_live = {
  .distance_cm = -1,
  .zone = "—",
  .status = "waiting for F756 serial…",
  .ack_status = "",
  .ack_armed = false,
  .lock = PTHREAD_MUTEX_INITIALIZER,
};

static int g_serial_fd = -1;
static pthread_mutex_t g_serial_lock = PTHREAD_MUTEX_INITIALIZER;

static void serial_fd_set(int fd)
{
  pthread_mutex_lock(&g_serial_lock);
  g_serial_fd = fd;
  pthread_mutex_unlock(&g_serial_lock);
}

static int serial_fd_get(void)
{
  int fd;
  pthread_mutex_lock(&g_serial_lock);
  fd = g_serial_fd;
  pthread_mutex_unlock(&g_serial_lock);
  return fd;
}

static void live_set_ok(float t, float h)
{
  pthread_mutex_lock(&g_live.lock);
  g_live.temp_c = t;
  g_live.hum_pct = h;
  g_live.has_temp = true;
  g_live.link_f756 = true;
  strncpy(g_live.zone, "SAFE", sizeof(g_live.zone) - 1);
  strncpy(g_live.status, "DHT22 OK · live from F756", sizeof(g_live.status) - 1);
  g_live.dirty = true;
  pthread_mutex_unlock(&g_live.lock);
}

static void live_set_fail(int err)
{
  char buf[96];
  snprintf(buf, sizeof(buf), "DHT22 fail err=%d · check OUT→D2", err);
  pthread_mutex_lock(&g_live.lock);
  g_live.link_f756 = false;
  strncpy(g_live.status, buf, sizeof(g_live.status) - 1);
  g_live.dirty = true;
  pthread_mutex_unlock(&g_live.lock);
}

static void live_set_status(const char *s)
{
  pthread_mutex_lock(&g_live.lock);
  strncpy(g_live.status, s, sizeof(g_live.status) - 1);
  g_live.dirty = true;
  pthread_mutex_unlock(&g_live.lock);
}

static void live_set_ack_status(const char *s)
{
  pthread_mutex_lock(&g_live.lock);
  strncpy(g_live.ack_status, s, sizeof(g_live.ack_status) - 1);
  g_live.ack_status[sizeof(g_live.ack_status) - 1] = '\0';
  g_live.ack_dirty = true;
  g_live.dirty = true;
  pthread_mutex_unlock(&g_live.lock);
}

static void live_set_sysmon(int cpu, int heap_free)
{
  if(cpu < 0) {
    cpu = 0;
  }
  if(cpu > 100) {
    cpu = 100;
  }
  if(heap_free < 0) {
    heap_free = 0;
  }
  if(heap_free > 100) {
    heap_free = 100;
  }
  pthread_mutex_lock(&g_live.lock);
  g_live.cpu_pct = cpu;
  g_live.heap_free_pct = heap_free;
  g_live.has_sysmon = true;
  g_live.dirty = true;
  pthread_mutex_unlock(&g_live.lock);
}

static int take_sysmon(const char *line)
{
  const char *cs = strstr(line, "CPU=");
  const char *hs = strstr(line, "HEAP=");
  int cpu = 0;
  int heap = 0;
  if(cs == NULL || hs == NULL) {
    return 0;
  }
  if(sscanf(cs, "CPU=%d", &cpu) != 1 || sscanf(hs, "HEAP=%d", &heap) != 1) {
    return 0;
  }
  live_set_sysmon(cpu, heap);
  return 1;
}

static void live_set_nfc(bool online, bool armed, const char *status_opt)
{
  pthread_mutex_lock(&g_live.lock);
  g_live.link_nfc = online;
  g_live.ack_armed = armed;
  if(status_opt != NULL) {
    strncpy(g_live.status, status_opt, sizeof(g_live.status) - 1);
    g_live.status[sizeof(g_live.status) - 1] = '\0';
  }
  g_live.dirty = true;
  pthread_mutex_unlock(&g_live.lock);
}

static int open_serial(const char *path)
{
  int fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
  if(fd < 0) {
    return -1;
  }

  struct termios tio;
  if(tcgetattr(fd, &tio) != 0) {
    close(fd);
    return -1;
  }
  cfmakeraw(&tio);
  cfsetispeed(&tio, B115200);
  cfsetospeed(&tio, B115200);
  tio.c_cflag |= (CLOCAL | CREAD);
  tio.c_cflag &= ~CSIZE;
  tio.c_cflag |= CS8;
  tio.c_cflag &= ~PARENB;
  tio.c_cflag &= ~CSTOPB;
  tio.c_cc[VMIN] = 0;
  tio.c_cc[VTIME] = 1;
  if(tcsetattr(fd, TCSANOW, &tio) != 0) {
    close(fd);
    return -1;
  }
  tcflush(fd, TCIOFLUSH);
  return fd;
}

static char *find_usbmodem(void)
{
  DIR *d = opendir("/dev");
  if(!d) {
    return NULL;
  }
  struct dirent *e;
  char *found = NULL;
  while((e = readdir(d)) != NULL) {
    if(strncmp(e->d_name, "cu.usbmodem", 11) == 0) {
      size_t n = strlen(e->d_name) + 6;
      found = (char *)malloc(n);
      if(found) {
        snprintf(found, n, "/dev/%s", e->d_name);
      }
      break;
    }
  }
  closedir(d);
  return found;
}

static void parse_line(const char *line)
{
  int t_i = 0;
  int t_f = 0;
  int h_i = 0;
  int h_f = 0;
  int err = 0;
  const char *tp;
  const char *hp;
  const char *uid;

  if(line == NULL || line[0] == '\0') {
    return;
  }

  tp = strstr(line, "TEMP=");
  hp = strstr(line, "HUM=");
  if(tp && hp && sscanf(tp, "TEMP=%d.%d", &t_i, &t_f) == 2 &&
     sscanf(hp, "HUM=%d.%d", &h_i, &h_f) == 2) {
    float t = (float)t_i + (float)t_f / 10.0f;
    if(t_i < 0) {
      t = (float)t_i - (float)t_f / 10.0f;
    }
    float h = (float)h_i + (float)h_f / 10.0f;
    live_set_ok(t, h);
    (void)take_sysmon(line);
    printf("[sim] UI <= %s\n", line);
    return;
  }
  if(take_sysmon(line)) {
    printf("[sim] UI <= %s\n", line);
    return;
  }
  if(sscanf(line, "DHT22 fail err=%d", &err) == 1) {
    live_set_fail(err);
    printf("[sim] %s\n", line);
    return;
  }
  if(strncmp(line, "NFC=OK", 6) == 0) {
    live_set_nfc(true, false, "NFC ready · tap badge to unlock ACK");
    live_set_ack_status("tap NFC badge");
    printf("[sim] %s\n", line);
    return;
  }
  if(strncmp(line, "NFC_DIAG", 8) == 0) {
    printf("[sim] %s\n", line);
    return;
  }
  if(strncmp(line, "NFC=FAIL", 8) == 0) {
    live_set_nfc(false, true, "NFC offline · ACK ungated");
    live_set_ack_status("NFC offline");
    printf("[sim] %s\n", line);
    return;
  }
  uid = strstr(line, "NFC_UID=");
  if(uid) {
    char msg[96];
    char badge[24];
    const char *p = uid + 8;
    size_t n = 0;
    while(p[n] != '\0' && p[n] != ' ' && p[n] != '\r' && n + 1 < sizeof(badge)) {
      badge[n] = p[n];
      n++;
    }
    badge[n] = '\0';
    operation_hmi_set_operator_id(badge);
    snprintf(msg, sizeof(msg), "badge %s · ACK unlocked 30s", badge);
    live_set_nfc(true, true, msg);
    live_set_ack_status("badge OK · ACK armed");
    printf("[sim] %s\n", line);
    return;
  }
  if(strcmp(line, "NFC_ARMED") == 0) {
    live_set_nfc(true, true, NULL);
    live_set_ack_status("ACK armed");
    printf("[sim] %s\n", line);
    return;
  }
  if(strcmp(line, "NFC_LOCKED") == 0) {
    live_set_nfc(true, false, "NFC session expired · tap badge again");
    live_set_ack_status("tap NFC badge");
    printf("[sim] %s\n", line);
    return;
  }
  if(strncmp(line, "ACK_OK", 6) == 0) {
    char who[24] = "F756";
    const char *uidf = strstr(line, "uid=");
    if(uidf) {
      uidf += 4;
      size_t n = 0;
      while(uidf[n] != '\0' && uidf[n] != ' ' && uidf[n] != '\r' && n + 1 < sizeof(who)) {
        who[n] = uidf[n];
        n++;
      }
      who[n] = '\0';
    }
    operation_hmi_confirm_ack(true, who);
    live_set_status("alarm acknowledged — condition stays until healthy");
    printf("[sim] %s\n", line);
    return;
  }
  if(strcmp(line, "ACK_DENIED") == 0) {
    live_set_ack_status("tap NFC badge first");
    live_set_status("ACK denied — badge required");
    printf("[sim] %s\n", line);
    return;
  }

  /* Show everything else so we can see if the port is alive */
  printf("[sim] rx: %s\n", line);
}

static void *serial_thread(void *arg)
{
  const char *path = (const char *)arg;
  char buf[256];
  size_t used = 0;

  for(;;) {
    int fd = open_serial(path);
    if(fd < 0) {
      char msg[128];
      snprintf(msg, sizeof(msg), "cannot open %s (close screen/cat?)", path);
      live_set_status(msg);
      sleep(2);
      continue;
    }
    live_set_status("listening for TEMP/HUM…");
    printf("[sim] serial open %s @ 115200\n", path);
    serial_fd_set(fd);

    used = 0;
    for(;;) {
      char tmp[64];
      ssize_t n = read(fd, tmp, sizeof(tmp));
      if(n < 0) {
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
          usleep(20000);
          continue;
        }
        break;
      }
      if(n == 0) {
        usleep(20000);
        continue;
      }
      for(ssize_t i = 0; i < n; i++) {
        char c = tmp[i];
        if(c == '\n' || c == '\r') {
          if(used > 0) {
            buf[used] = '\0';
            parse_line(buf);
            used = 0;
          }
        } else if(used + 1 < sizeof(buf)) {
          buf[used++] = c;
        } else {
          used = 0;
        }
      }
    }
    serial_fd_set(-1);
    close(fd);
    live_set_status("serial disconnected — retrying…");
    sleep(1);
  }
  return NULL;
}

static void flush_live_to_ui(void)
{
  pthread_mutex_lock(&g_live.lock);
  if(!g_live.dirty) {
    pthread_mutex_unlock(&g_live.lock);
    return;
  }
  float t = g_live.temp_c;
  float h = g_live.hum_pct;
  int d = g_live.distance_cm;
  int cpu = g_live.cpu_pct;
  int heap_free = g_live.heap_free_pct;
  char zone[16];
  char status[96];
  char ack_status[48];
  bool has = g_live.has_temp;
  bool has_sysmon = g_live.has_sysmon;
  bool link = g_live.link_f756;
  bool link_nfc = g_live.link_nfc;
  bool ack_armed = g_live.ack_armed;
  bool ack_dirty = g_live.ack_dirty;
  strncpy(zone, g_live.zone, sizeof(zone));
  strncpy(status, g_live.status, sizeof(status));
  strncpy(ack_status, g_live.ack_status, sizeof(ack_status));
  g_live.dirty = false;
  g_live.ack_dirty = false;
  pthread_mutex_unlock(&g_live.lock);

  if(has) {
    operation_hmi_set_temp_c(t);
    operation_hmi_set_hum_pct(h);
  }
  if(has_sysmon) {
    operation_hmi_set_cpu_pct(cpu);
    operation_hmi_set_heap_free_pct(heap_free);
  }
  operation_hmi_set_distance_cm(d);
  operation_hmi_set_zone(zone);
  operation_hmi_set_status(status);
  operation_hmi_set_link_f756(link);
  operation_hmi_set_link_nfc(link_nfc);
  operation_hmi_set_ack_armed(ack_armed);
  if(ack_dirty) {
    operation_hmi_set_ack_status(ack_status);
  }
}

static ssize_t serial_write_all(int fd, const char *data, size_t len)
{
  size_t off = 0;
  while(off < len) {
    struct pollfd pfd = {.fd = fd, .events = POLLOUT};
    if(poll(&pfd, 1, 500) <= 0) {
      return -1;
    }
    ssize_t n = write(fd, data + off, len - off);
    if(n < 0) {
      if(errno == EAGAIN || errno == EINTR) {
        continue;
      }
      return -1;
    }
    if(n == 0) {
      return -1;
    }
    off += (size_t)n;
  }
  (void)tcdrain(fd);
  return (ssize_t)off;
}

static void on_ack(void)
{
  const char msg[] = "ACK\r\n";
  int fd = serial_fd_get();

  if(fd < 0) {
    operation_hmi_confirm_ack(true, "LOCAL");
    printf("[sim] ACK: local Quittierung (no serial)\n");
    return;
  }

  live_set_ack_status("sending…");
  operation_hmi_set_ack_status("sending…");

  pthread_mutex_lock(&g_serial_lock);
  ssize_t n = serial_write_all(g_serial_fd, msg, sizeof(msg) - 1U);
  int err = errno;
  pthread_mutex_unlock(&g_serial_lock);

  if(n < 0) {
    live_set_ack_status("send failed");
    operation_hmi_set_ack_status("send failed");
    printf("[sim] ACK write error: %s\n", strerror(err));
    return;
  }

  live_set_ack_status("waiting ACK…");
  operation_hmi_set_ack_status("waiting ACK…");
  printf("[sim] ACK -> F756 (%zd bytes)\n", n);
}

static Uint32 tick_cb(Uint32 interval, void *param)
{
  LV_UNUSED(param);
  lv_tick_inc(interval);
  return interval;
}

int main(int argc, char **argv)
{
  char *auto_port = NULL;
  const char *port = NULL;

  if(argc >= 2) {
    port = argv[1];
  } else {
    auto_port = find_usbmodem();
    port = auto_port;
  }

  /* Keep pixels sharp on macOS Retina: no HiDPI stretch + nearest-neighbor upscale. */
  SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "1");
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0"); /* nearest, not bilinear blur */

  if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    free(auto_port);
    return 1;
  }

  lv_init();
  SDL_AddTimer(1, tick_cb, NULL);

  lv_display_t *disp = lv_sdl_window_create(HOR_RES, VER_RES);
  if(disp == NULL) {
    fprintf(stderr, "lv_sdl_window_create failed\n");
    free(auto_port);
    return 1;
  }
  /* Integer 2× zoom — crisp on desktop, same 480×320 UI content as the panel. */
  lv_sdl_window_set_zoom(disp, 2);
  lv_sdl_window_set_resizeable(disp, false);
  lv_sdl_window_set_title(disp, OPERATION_HMI_PRODUCT_NAME);
  lv_sdl_mouse_create();

  operation_hmi_set_ack_callback(on_ack);
  operation_hmi_create();
  operation_hmi_set_distance_cm(-1);

  pthread_t thr;
  if(port) {
    printf("[sim] using serial %s\n", port);
    printf("[sim] close Terminal screen/cat on this port first\n");
    pthread_create(&thr, NULL, serial_thread, (void *)port);
    pthread_detach(thr);
  } else {
    const int spark[] = {
        52, 61, 74, 58, 80, 69, 88, 63, 77, 55, 82, 70,
        91, 66, 73, 57, 84, 71, 79, 60, 86, 68, 75, 72
    };
    operation_hmi_set_temp_c(24.1f);
    operation_hmi_set_hum_pct(48.0f);
    for(unsigned i = 0; i < sizeof(spark) / sizeof(spark[0]); i++) {
      operation_hmi_set_distance_cm(spark[i]);
    }
    operation_hmi_set_link_eth(true);
    operation_hmi_set_cpu_pct(18);
    operation_hmi_set_heap_free_pct(62);
    operation_hmi_log_event(OPERATION_HMI_EVT_OK, "Sensor OK");
    operation_hmi_set_status("Demo — connect F756 for live data");
    fprintf(stderr, "[sim] no serial port. Pass one: %s /dev/cu.usbmodemXXXX\n", argv[0]);
  }

  printf("[sim] LVGL 480x320 @ 2× nearest — crisp preview of the panel UI\n");

  while(1) {
    flush_live_to_ui();
    lv_timer_handler();
    SDL_Delay(2);
  }

  free(auto_port);
  return 0;
}
