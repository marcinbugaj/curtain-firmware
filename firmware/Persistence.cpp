#include "Persistence.h"

#include <cstdio>

#include "hardware/flash.h"
#include "hardware/sync.h"

extern char __flash_binary_end;
size_t end = (size_t)&__flash_binary_end;

constexpr size_t alignTo(size_t pointer, size_t alignment) {
  return ((pointer + alignment - 1) / alignment) * alignment;
}

constexpr size_t alignToFlashSectorSize(size_t size) {
  return alignTo(size, FLASH_SECTOR_SIZE);
}

constexpr size_t alignToFlashPageSize(size_t size) {
  return alignTo(size, FLASH_PAGE_SIZE);
}

const uint8_t *ConfigAddress = (uint8_t *)(alignToFlashSectorSize(end));

constexpr const char Magical[] = "magical string";

typedef struct {
  Config_t config;
  uint8_t magical[sizeof(Magical)];
  uint8_t _padding[256 - sizeof(Config_t) - sizeof(Magical)];
} __attribute__((packed)) ConfigPadded_t;

static_assert(sizeof(ConfigPadded_t) == 256,
              "ConfigPadded_t has size that is not multiple of 256");

std::optional<std::reference_wrapper<const Config_t>> readConfig() {
  auto p = (const ConfigPadded_t *const)ConfigAddress;
  if (memcmp(Magical, p->magical, sizeof(Magical)) != 0) {
    return {};
  }
  return std::cref(p->config);
}

// assumes that core 1 is not running
bool saveConfig(const Config_t &_config) {
  ConfigPadded_t config;
  memset(&config, 0, sizeof(Config_t));
  config.config = _config;
  memcpy(config.magical, Magical, sizeof(Magical));

  fflush(stdout);

  auto saved = save_and_disable_interrupts();

  const size_t FLASH_TARGET_OFFSET = (size_t)ConfigAddress - XIP_BASE;

  flash_range_erase(FLASH_TARGET_OFFSET,
                    alignToFlashSectorSize(sizeof(ConfigPadded_t)));
  flash_range_program(FLASH_TARGET_OFFSET, (uint8_t *)&config,
                      sizeof(ConfigPadded_t));

  restore_interrupts(saved);

  auto mConfig = readConfig();
  if (!mConfig.has_value()) {
    return false;
  }

  bool success =
      memcmp(&_config, &mConfig.value().get(), sizeof(Config_t)) == 0;

  return success;
}