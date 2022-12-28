#include "UARTImpl.h"

#include "hardware/gpio.h"
#include "hardware/uart.h"

namespace {

class UARTImpl : public tmc2209::UART {
public:
  UARTImpl() {
    uart_init(uart1, 115200);
    gpio_set_function(4, GPIO_FUNC_UART);
    gpio_set_function(5, GPIO_FUNC_UART);
  }

  void read(uint8_t *dest, size_t size) {
    uart_read_blocking(uart1, dest, size);
  }
  void write(const uint8_t *src, size_t size) {
    uart_write_blocking(uart1, src, size);
  }
  bool isReadable() const { return uart_is_readable(uart1); }
};

} // namespace

std::unique_ptr<tmc2209::UART> createUART() {
  return std::make_unique<UARTImpl>();
}