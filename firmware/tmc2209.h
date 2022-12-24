#pragma once

#include <stddef.h>
#include <stdint.h>

#include <memory>

namespace tmc2209 {
class UART {
public:
  virtual ~UART() = default;
  virtual void read(uint8_t *dest, size_t size) = 0;
  virtual void write(const uint8_t *src, size_t size) = 0;
  virtual bool isReadable() const = 0;
};

using UARTPtr = std::unique_ptr<UART>;

class Driver {
public:
  virtual ~Driver() = default;

  virtual void initialize() = 0;
};

std::unique_ptr<Driver> create(UARTPtr);

} // namespace tmc2209