#include "hardware/gpio.h"
#include "hardware/rtc.h"
#include "hardware/watchdog.h"
#include "pico/cyw43_arch.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/types.h"
#include "pico/util/datetime.h"

#include "lwip/apps/httpd.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#include "dhcp.h"
#include "picow_ntp_client.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdio.h>
#include <stdlib.h>
#include <string>

#include "ENNPinImpl.h"
#include "ParseConfig.h"
#include "Persistence.h"
#include "Server.h"
#include "StepPinSqWaveImpl.h"
#include "UARTImpl.h"
#include "UrlDecode.h"
#include "Utils.h"
#include "tmc2209.h"

#include "wiring.h"

static int diagReported = 0;

void callback(uint gpio, uint32_t event_mask) {
  if (gpio != DIAG_PIN || event_mask != GPIO_IRQ_EDGE_RISE) {
    abort();
  }
  diagReported++;
}

void configure_DIAG() {
  gpio_init(DIAG_PIN);
  gpio_set_dir(DIAG_PIN, GPIO_IN);
  gpio_set_irq_enabled_with_callback(DIAG_PIN, GPIO_IRQ_EDGE_RISE, true,
                                     &callback);
}

static volatile bool alarm = false;
static void alarm_callback(void) { alarm = true; }

void startRTC(struct tm *t) {
  rtc_init();

  datetime_t date;
  date.year = t->tm_year;
  date.month = t->tm_mon + 1;
  date.day = t->tm_mday;
  date.hour = t->tm_hour;
  date.min = t->tm_min;
  date.sec = 0;
  date.dotw = t->tm_wday;

  rtc_set_datetime(&date);

  const Config_t &c = readConfig().value();

#if 1
  datetime_t alarm = {.year = -1,
                      .month = -1,
                      .day = -1,
                      .dotw = -1,
                      .hour = (int8_t)c.hour,
                      .min = (int8_t)c.minute,
                      .sec = 00};
#else
  datetime_t alarm = date;
  alarm.min += 1;
#endif

  rtc_set_alarm(&alarm, &alarm_callback);
}

void connectToNetwork() {
  cyw43_arch_enable_sta_mode();

  const Config_t &c = readConfig().value();

  if (cyw43_arch_wifi_connect_timeout_ms(c.ssid, c.password,
                                         CYW43_AUTH_WPA2_AES_PSK, 10000)) {
    printf("Failed to connect to WIFI network\n");
    abort();
  }
}

void printCurrentDate() {
  datetime_t t = {0};
  rtc_get_datetime(&t);
  char datetime_buf[256];
  char *datetime_str = &datetime_buf[0];
  datetime_to_str(datetime_str, sizeof(datetime_buf), &t);
  printf("Alarm Fired At %s\n", datetime_str);
  stdio_flush();
}

static void initializeCYW43() {
  if (cyw43_arch_init()) {
    printf("Failed to initialise CYW43 driver\n");
    abort();
  }
}

void doJob() {
  initializeCYW43();

  connectToNetwork();

  auto t = timeFixViaNTP();

  cyw43_arch_deinit();

  configure_DIAG();
  auto uart = createUART();
  auto wave = createStepPinSqWave();
  auto enn = createENNPin();
  auto driver = std::move(
      tmc2209::create(std::move(uart), std::move(wave), std::move(enn)));
  driver->initialize();

  startRTC(t); // only after driver is initialized

  while (true) {
    if (alarm) {
      alarm = false;

      printCurrentDate();

      const auto prevDiag = diagReported;

      driver->enable();
      sleep_ms(100);

      const auto start = get_absolute_time();
      driver->move(0);

      while (prevDiag == diagReported &&
             (absolute_time_diff_us(start, get_absolute_time()) < 16e6)) {
        sleep_ms(100);
      }

      driver->stop();

      sleep_ms(100);
      driver->disable();
    }

    sleep_ms(5000);
  }
}

const Config_t &readConfigUnsafe() {
  auto c = readConfig();
  if (!c.has_value()) {
    error_out("Cannot read config");
  }
  return c.value().get();
}

void rebootIntoNormalMode() {
  watchdog_enable(100, 1);
  for (;;) {
  }
}

void strategy() {
  if (watchdog_caused_reboot()) {
    printf("Rebooting into normal mode\n");

    const Config_t &c = readConfigUnsafe();
    printf("Config: %s\n", to_string(c).c_str());

    doJob();
  } else {
    printf("Booting into config mode\n");

    auto form = runServerForXMintes();

    auto newConfig = parseConfig(form);

    if (newConfig.has_value()) {
      printf("Saving config: %s\n", to_string(newConfig.value()).c_str());
      if (!saveConfig(newConfig.value())) {
        error_out("Could not save config");
      } else {
        printf("Config saved\n");
      }
    }

    readConfigUnsafe();

    rebootIntoNormalMode();
  }
}

int main() {
  stdio_init_all();

  sleep_ms(1000);

  strategy();
}
