#pragma once

#include <cstring>
#include <stdint.h>
#include <string>

typedef struct _Config_t {
  // all these must be null terminated strings
  char ssid[64];
  char password[64];
  uint8_t hour; /* hour 0-23 */
  uint8_t minute;

  bool operator==(const _Config_t &c) const {
    return strcmp(ssid, c.ssid) == 0 && strcmp(password, c.password) == 0 &&
           hour == c.hour && minute == c.minute;
  }
} Config_t;

static std::string to_string(const Config_t &c) {
  std::string s;
  s += "(";
  s += c.ssid;
  s += ",";
  s += c.password;
  s += ",";
  s += std::to_string(c.hour);
  s += ":";
  s += std::to_string(c.minute);
  s += ")";

  return s;
}
