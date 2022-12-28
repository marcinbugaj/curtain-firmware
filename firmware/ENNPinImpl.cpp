#include "ENNPinImpl.h"

#include "hardware/gpio.h"

#include "wiring.h"

namespace {

class ENNPinImpl : public tmc2209::ENNPin {
public:
  ENNPinImpl() {
    gpio_init(ENN_PIN);
    gpio_set_dir(ENN_PIN, GPIO_OUT);
  }
  void set(bool state) { gpio_put(ENN_PIN, state); }
};
} // namespace

std::unique_ptr<tmc2209::ENNPin> createENNPin() {
  return std::make_unique<ENNPinImpl>();
}