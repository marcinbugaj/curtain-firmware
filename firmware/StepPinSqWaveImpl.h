#pragma once

#include "tmc2209.h"

#include <memory>

std::unique_ptr<tmc2209::StepPinSqWave> createStepPinSqWave();