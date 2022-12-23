#include "tmc2209.h"

namespace tmc2209 {
namespace {

class DriverImpl : public Driver {
public:
  virtual void initialize() {}
};

} // namespace

} // namespace tmc2209