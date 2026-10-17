#pragma once
#include <cstdint>
struct NetworkConfig { bool dhcp=true; char ip[16]="", gateway[16]="", mask[16]="", dns[16]=""; };
struct DeviceConfig {
    uint32_t version=1;
    char name[33]="ntp-server";
    NetworkConfig network{}, previous_network{};
    bool network_pending=false, network_attempted=false;
    uint8_t brightness=100, dim_brightness=80;
    uint32_t dim_seconds=60;
    // Preserve the existing NVS record layout; unused bytes are zeroed on load.
    uint8_t reserved_v1[328]{};
    uint8_t salt[16]{}, password_hash[32]{};
};
// Startup config is immutable. Display settings use atomic live values.
const DeviceConfig &device_config();
void device_config_init();
void device_config_read(DeviceConfig &out);
bool device_config_save(DeviceConfig &value);
bool device_config_confirm_network();
bool device_config_pending_network();
bool device_config_password(const char *password, const DeviceConfig &config);
bool device_config_set_password(DeviceConfig &config, const char *password);
uint8_t device_display_brightness(bool dim);
uint32_t device_display_dim_seconds();
void device_config_restart();
void device_update_active(bool active);
bool device_update_is_active();
void device_web_ready();
