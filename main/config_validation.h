#pragma once
#include "device_config.h"
#include <cstring>
#include <cctype>
inline bool parse_ipv4(const char *s, uint32_t &ip) {
    ip=0;
    for(int part=0;part<4;++part) {
        unsigned n=0, digits=0;
        while(*s>='0'&&*s<='9') { n=n*10+unsigned(*s++-'0'); if(++digits>3||n>255)return false; }
        if(!digits)return false;
        ip=(ip<<8)|n;
        if(part<3){if(*s++!='.')return false;}else if(*s)return false;
    }
    return true;
}
inline bool unicast_ip(uint32_t ip) { return ip && (ip>>24)!=127 && (ip>>24)<224 && (ip>>24)!=0 && ip!=0xffffffffU; }
inline bool network_equal(const NetworkConfig &a,const NetworkConfig &b) {
    return a.dhcp==b.dhcp&&!std::strcmp(a.ip,b.ip)&&!std::strcmp(a.gateway,b.gateway)&&!std::strcmp(a.mask,b.mask)&&!std::strcmp(a.dns,b.dns);
}
inline const char *validate_config(const DeviceConfig &c) {
    size_t n=std::strlen(c.name);
    if(!n||n>32||c.name[0]=='-'||c.name[n-1]=='-')return "Device name: 1-32 characters, no leading or trailing hyphen.";
    for(char x: c.name) {if(!x)break;if(!std::isalnum((unsigned char)x)&&x!='-')return "Device name: letters, digits and hyphens only.";}
    if(!c.brightness||!c.dim_brightness||c.dim_brightness>c.brightness||c.dim_seconds>86400)return "Invalid display brightness or dimming delay.";
    if(!c.network.dhcp) {
        uint32_t ip,gw,mask;
        if(!parse_ipv4(c.network.ip,ip)||!unicast_ip(ip)||!parse_ipv4(c.network.gateway,gw)||!unicast_ip(gw)||!parse_ipv4(c.network.mask,mask))return "Invalid IPv4 address, gateway or subnet mask.";
        uint32_t hosts=~mask;
        if(!mask||hosts<3||(hosts&(hosts+1))||(ip&mask)!=(gw&mask)||(ip&hosts)==0||(ip&hosts)==hosts||(gw&hosts)==0||(gw&hosts)==hosts||ip==gw)return "Subnet mask, host address and gateway do not match.";
    }
    if(c.network.dns[0]){uint32_t dns;if(!parse_ipv4(c.network.dns,dns)||!unicast_ip(dns))return "Invalid DNS address.";}
    return nullptr;
}
