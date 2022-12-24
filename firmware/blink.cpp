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

bool repeating_timer_callback(struct repeating_timer *t) {
  gpio_put(STEP_PIN, current);
  current = !current;

  return true;
}

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
  gpio_put(ENN_PIN, 0);
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
  auto driver = tmc2209::create(std::move(uart));
  driver->initialize();

  struct repeating_timer timer;
  add_repeating_timer_us(156 /*312*/, repeating_timer_callback, NULL, &timer);

  while (true) {
    // dump<TSTEP_t>();
    // dump<SG_RESULT_t>();
    if (diagReported) {
      printf("DIAG: %d\n", diagReported);
    }
    sleep_ms(500);
  }
}
