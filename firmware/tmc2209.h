#pragma once

#include <stddef.h>
#include <stdint.h>

#include <memory>

namespace tmc2209 {
class UART {
public:
  virtual ~UART() = default;
  virtual void read(uint8_t *dest, size_t size) = 0;
  virtual void write(uint8_t *src, size_t size) = 0;
};

class Driver {
public:
  virtual ~Driver() = default;

  virtual void initialize() = 0;
};

} // namespace tmc2209