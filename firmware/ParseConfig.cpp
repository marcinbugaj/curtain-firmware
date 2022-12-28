#include "ParseConfig.h"

#include "UrlDecode.h"

std::optional<Config_t> parseConfig(const UrlEncodedSettingsForm_t &form) {
  auto ssid = urldecode(form.ssid);
  auto password = urldecode(form.password);
  auto wakeup = urldecode(form.wakeup);

  if (ssid.empty() || password.empty() || wakeup.empty()) {
    return {};
  }

  if (ssid.length() >= sizeof(Config_t::ssid)) {
    return {};
  }

  if (password.length() >= sizeof(Config_t::password)) {
    return {};
  }

  if (wakeup.at(2) != ':') {
    return {};
  }

  auto toInt = [](char c1, char c2) {
    char t[3];
    t[2] = 0;
    t[0] = c1;
    t[1] = c2;

    return atoi(t);
  };

  const auto hour = toInt(wakeup.at(0), wakeup.at(1));
  const auto minute = toInt(wakeup.at(3), wakeup.at(4));

  if (hour > 23 || hour < 0) {
    return {};
  }

  if (minute > 59 || minute < 0) {
    return {};
  }

  Config_t c;

  strcpy(c.ssid, ssid.c_str());
  strcpy(c.password, password.c_str());
  c.hour = hour;
  c.minute = minute;

  return c;
}