#pragma once

#include "Config.h"

#include <functional>
#include <optional>

std::optional<std::reference_wrapper<const Config_t>> readConfig();

bool saveConfig(const Config_t &_config);