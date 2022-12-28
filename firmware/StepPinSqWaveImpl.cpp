#include "StepPinSqWaveImpl.h"

#include "hardware/gpio.h"
#include "pico/time.h"

#include "wiring.h"

namespace {
class StepPinSqWaveImpl : public tmc2209::StepPinSqWave {
  struct repeating_timer timer;
  const uint pin;

  bool current = false;

public:
  StepPinSqWaveImpl(uint pin) : pin(pin) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
  }

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
} // namespace

std::unique_ptr<tmc2209::StepPinSqWave> createStepPinSqWave() {
  return std::make_unique<StepPinSqWaveImpl>(STEP_PIN);
}