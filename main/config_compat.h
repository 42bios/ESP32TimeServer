#pragma once
#include "device_config.h"
#include <cstring>

// Former optional-service bytes must not retain credentials after upgrade.
inline bool clear_legacy_config_bytes(DeviceConfig &config) {
    bool changed = false;
    for (uint8_t byte : config.reserved_v1) changed |= byte != 0;
    if (changed) std::memset(config.reserved_v1, 0, sizeof(config.reserved_v1));
    return changed;
}
