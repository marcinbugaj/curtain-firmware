/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/uart.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/types.h"

#include <cstdint>
#include <stdio.h>
#include <stdlib.h>

#include "tmc2209.h"

const uint ENN_PIN = 8;
const uint STEP_PIN = 6;
const uint DIAG_PIN = 7;

bool current = false;

class UARTImpl : public tmc2209::UART {
public:
  void read(uint8_t *dest, size_t size) {
    uart_read_blocking(uart1, dest, size);
  }
  void write(const uint8_t *src, size_t size) {
    uart_write_blocking(uart1, src, size);
  }
  bool isReadable() const { return uart_is_readable(uart1); }
};

class StepPinSqWaveImpl : public tmc2209::StepPinSqWave {
  struct repeating_timer timer;
  const uint pin;

public:
  StepPinSqWaveImpl(uint pin) : pin(pin) {}

  virtual void start(size_t periodUs) {
    add_repeating_timer_us(periodUs /*312*/, _callback, this, &timer);
  }
  virtual void stop() { cancel_repeating_timer(&timer); }

  static bool _callback(struct repeating_timer *t) {
    auto impl = (StepPinSqWaveImpl *)t->user_data;

    return impl->callback(t);
  }

  bool callback(struct repeating_timer *t) {
    gpio_put(pin, current);
    current = !current;
    return true;
  }
};

class GPIO_OutImpl : public tmc2209::GPIO_Out {
public:
  void set(bool state) { gpio_put(ENN_PIN, state); }
};

static int diagReported = 0;

void callback(uint gpio, uint32_t event_mask) {
  if (gpio != DIAG_PIN || event_mask != GPIO_IRQ_EDGE_RISE) {
    abort();
  }
  diagReported++;
}

void configure_ENN() {
  gpio_init(ENN_PIN);
  gpio_set_dir(ENN_PIN, GPIO_OUT);
}

void configure_STEP() {
  gpio_init(STEP_PIN);
  gpio_set_dir(STEP_PIN, GPIO_OUT);
}

void configure_DIAG() {
  gpio_init(DIAG_PIN);
  gpio_set_dir(DIAG_PIN, GPIO_IN);
  gpio_set_irq_enabled_with_callback(DIAG_PIN, GPIO_IRQ_EDGE_RISE, true,
                                     &callback);
}

void configure_UART() {
  uart_init(uart1, 115200);
  gpio_set_function(4, GPIO_FUNC_UART);
  gpio_set_function(5, GPIO_FUNC_UART);
}

int main() {
  stdio_init_all();

  configure_UART();

#if 1
  configure_ENN();
  configure_STEP();
  configure_DIAG();
#endif

  printf("Board started!\n");

  auto uart = std::make_unique<UARTImpl>();
  auto wave = std::make_unique<StepPinSqWaveImpl>(STEP_PIN);
  auto enn = std::make_unique<GPIO_OutImpl>();
  auto driver =
      tmc2209::create(std::move(uart), std::move(wave), std::move(enn));
  driver->initialize();

  driver->move(0);

  while (true) {

    driver->debug();
    if (diagReported) {
      printf("DIAG: %d\n", diagReported);
    }
    sleep_ms(500);
  }
}
