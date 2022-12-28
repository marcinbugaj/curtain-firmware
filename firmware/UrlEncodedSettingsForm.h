#pragma once

typedef struct {
  char ssid[128];
  char password[128];
  char wakeup[32]; // HH:MM + null
} UrlEncodedSettingsForm_t;