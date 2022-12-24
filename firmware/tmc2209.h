#pragma once

#include <stddef.h>
#include <stdint.h>

#include <memory>

namespace tmc2209 {
struct UART {
  virtual ~UART() = default;
  virtual void read(uint8_t *dest, size_t size) = 0;
  virtual void write(const uint8_t *src, size_t size) = 0;
  virtual bool isReadable() const = 0;
};

struct GPIO_Out {
  virtual ~GPIO_Out() = default;
  virtual void set(bool) = 0;
};

struct StepPinSqWave {
  virtual void start(size_t periodUs) = 0;
  virtual void stop() = 0;
};

using UARTPtr = std::unique_ptr<UART>;
using StepPinSqWavePtr = std::unique_ptr<StepPinSqWave>;
using GPIO_OutPtr = std::unique_ptr<GPIO_Out>;

class Driver {
public:
  virtual ~Driver() = default;

  virtual void initialize() = 0;
  virtual void onDiagPin() = 0;
  virtual void debug() = 0;

  virtual void move(uint32_t rpm) = 0;
  virtual void stop() = 0;
};

std::unique_ptr<Driver> create(UARTPtr, StepPinSqWavePtr, GPIO_OutPtr);

} // namespace tmc2209