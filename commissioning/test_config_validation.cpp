#include "../main/config_validation.h"
#include <cassert>
#include <initializer_list>
#include <cstdio>
#include <cstring>
int main(){
 DeviceConfig c;assert(validate_config(c)==nullptr);
 c.network.dhcp=false;std::strcpy(c.network.ip,"192.168.30.146");std::strcpy(c.network.gateway,"192.168.30.1");std::strcpy(c.network.mask,"255.255.255.0");assert(!validate_config(c));
 std::strcpy(c.network.ip,"192.168.30.0");assert(validate_config(c));
 std::strcpy(c.network.ip,"192.168.30.255");assert(validate_config(c));
 std::strcpy(c.network.ip,"192.168.31.10");assert(validate_config(c));
 std::strcpy(c.network.ip,"192.168.30.146");std::strcpy(c.network.mask,"255.0.255.0");assert(validate_config(c));
 std::strcpy(c.network.mask,"255.255.255.0");c.dim_brightness=101;assert(validate_config(c));c.dim_brightness=80;
 std::strcpy(c.name,"bad name");assert(validate_config(c));std::strcpy(c.name,"ntp-server");assert(!validate_config(c));
 uint32_t ip;for(const char *s:{"1.2.3","1.2.3.256","1.2.3.4x","-1.2.3.4","1..3.4","1234.2.3.4"})assert(!parse_ipv4(s,ip));
 puts("Configuration validation passed");
}
