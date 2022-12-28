#pragma once

#include <optional>

#include "Config.h"
#include "UrlEncodedSettingsForm.h"

std::optional<Config_t> parseConfig(const UrlEncodedSettingsForm_t &);