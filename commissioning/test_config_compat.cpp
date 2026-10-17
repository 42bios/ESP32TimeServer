#include "../main/config_compat.h"
#include <cassert>
#include <cstddef>
#include <cstring>

// Actual v1 on-flash field order, before optional-service removal.
struct LegacyV1 {
    uint32_t version=1;
    char name[33]="custom-ntp";
    NetworkConfig network{},previous_network{};
    bool network_pending=false,network_attempted=false;
    uint8_t brightness=90,dim_brightness=70;
    uint32_t dim_seconds=120;
    bool service_enabled=true;
    char host[65]="old-broker",user[65]="old-user",password[129]="old-secret",topic[65]="old-topic";
    uint16_t port=1883;
    uint8_t salt[16]{},password_hash[32]{};
};
int main(){
    static_assert(sizeof(LegacyV1)==sizeof(DeviceConfig),"v1 record size");
    static_assert(offsetof(LegacyV1,salt)==offsetof(DeviceConfig,salt),"credential offset");
    LegacyV1 legacy;
    legacy.salt[3]=42;legacy.password_hash[7]=91;
    std::strcpy(legacy.network.ip,"192.168.1.50");legacy.network.dhcp=false;
    DeviceConfig current;
    std::memcpy(&current,&legacy,sizeof(current));
    assert(clear_legacy_config_bytes(current));
    assert(!std::strcmp(current.name,"custom-ntp"));
    assert(!current.network.dhcp&&!std::strcmp(current.network.ip,"192.168.1.50"));
    assert(current.brightness==90&&current.dim_brightness==70&&current.dim_seconds==120);
    assert(current.salt[3]==42&&current.password_hash[7]==91);
    for(uint8_t byte:current.reserved_v1)assert(byte==0);
    assert(!clear_legacy_config_bytes(current));
}
